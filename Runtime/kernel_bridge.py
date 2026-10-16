"""Private JSON-lines bridge to a real ipykernel in this exact interpreter.

stdin/stdout belong to Winip, never to executed code. Complete bounded output
bundles are published after the matching shell reply and IOPub idle message.
"""
import json
import os
from pathlib import Path
from queue import Empty
import subprocess
import sys
import tempfile
import time
import threading
import ctypes

LIMIT = 1024 * 1024
EMIT_LOCK = threading.Lock()


def emit(value):
    with EMIT_LOCK:
        sys.stdout.buffer.write(json.dumps(value, ensure_ascii=True).encode('ascii') + b'\n')
        sys.stdout.buffer.flush()


def execute(manager, client, code):
    request = client.execute(code, allow_stdin=False, stop_on_error=True)
    outputs = []
    size = 0
    clear_pending = False
    displays = {}
    while True:
        try:
            message = client.get_iopub_msg(timeout=1)
        except Empty:
            if not manager.is_alive():
                raise RuntimeError('Kernel exited during execution')
            continue
        if message.get('parent_header', {}).get('msg_id') != request:
            continue
        kind, content = message['msg_type'], message['content']
        if kind == 'status' and content['execution_state'] == 'idle':
            break
        if kind == 'clear_output':
            clear_pending = content.get('wait', False)
            if not clear_pending:
                outputs, displays, size = [], {}, 0
            continue
        if kind not in ('stream', 'error', 'execute_result', 'display_data', 'update_display_data'):
            continue
        if clear_pending:
            outputs, displays, size, clear_pending = [], {}, 0, False
        output = dict(content)
        display = output.pop('transient', {}).get('display_id')
        output['output_type'] = 'display_data' if kind == 'update_display_data' else kind
        # Never silently truncate output. A bounded first slice reports failure
        # and terminates this session when an output bundle exceeds the limit.
        size += len(json.dumps(output, ensure_ascii=True))
        if size > LIMIT or len(outputs) >= 4096:
            raise RuntimeError('Cell output exceeds the 1 MiB / 4096 message limit; kernel stopped')
        if kind == 'update_display_data':
            for index in displays.get(display, []):
                outputs[index] = output
        else:
            if display:
                displays.setdefault(display, []).append(len(outputs))
            outputs.append(output)
    deadline = time.monotonic() + 10
    while True:
        reply = client.get_shell_msg(timeout=max(0.01, deadline-time.monotonic()))
        if reply.get('parent_header', {}).get('msg_id') == request:
            return {'event': 'result', 'outputs': outputs,
                    'execution_count': reply['content'].get('execution_count'),
                    'status': reply['content']['status']}
        if time.monotonic() > deadline:
            raise RuntimeError('Missing execution reply')


def main():
    manager = client = None
    try:
        import ipykernel  # Fail explicitly in the selected environment.
        from jupyter_client import KernelManager
        from jupyter_client.kernelspec import KernelSpec, KernelSpecManager
        class SelectedKernelSpecs(KernelSpecManager):
            def get_kernel_spec(self, name):
                return KernelSpec(argv=[sys.executable, "-m", "ipykernel_launcher", "-f", "{connection_file}"],
                                  display_name="Winip Python", language="python")
        runtime = Path(sys.argv[1])
        runtime.mkdir(parents=True, exist_ok=True)
        # All generated connection/profile/history files belong to user state.
        with tempfile.TemporaryDirectory(prefix='session-', dir=runtime) as session:
            env = os.environ.copy()
            env.update(IPYTHONDIR=str(Path(session)/'ipython'),
                       JUPYTER_RUNTIME_DIR=session, PYTHONDONTWRITEBYTECODE='1')
            manager = KernelManager(connection_file=str(Path(session)/'connection.json'),
                                    kernel_spec_manager=SelectedKernelSpecs())
            try:
                manager.start_kernel(cwd=sys.argv[2], env=env, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.DEVNULL)
                client = manager.blocking_client()
                client.start_channels(hb=False)
                client.wait_for_ready(timeout=30)
                control_stop = threading.Event()
                event_handle = int(sys.argv[3]) if len(sys.argv) > 3 else 0
                def controls():
                    wait = ctypes.windll.kernel32.WaitForSingleObject
                    wait.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
                    wait.restype = ctypes.c_ulong
                    while not control_stop.is_set():
                        if wait(event_handle, 0xFFFFFFFF) != 0:
                            emit({'event': 'error', 'message': 'Interrupt control handle failed'})
                            return
                        if not control_stop.is_set():
                            try:
                                manager.interrupt_kernel()
                                emit({'event': 'interrupt_sent'})
                            except Exception as error:
                                emit({'event': 'error', 'message': 'Interrupt failed: ' + str(error)})
                control_thread = threading.Thread(target=controls) if event_handle else None
                if control_thread:
                    control_thread.start()
                emit({'event': 'ready', 'executable': sys.executable,
                      'ipykernel': ipykernel.__version__})
                while True:
                    line = sys.stdin.buffer.readline(8 * LIMIT + 1)
                    if not line:
                        break
                    if len(line) > 8 * LIMIT:
                        raise RuntimeError('Execution request exceeds bridge limit')
                    command = json.loads(line)
                    if command['command'] == 'shutdown':
                        break
                    if command['command'] != 'execute' or not isinstance(command['source'], str):
                        raise RuntimeError('Invalid bridge command')
                    emit(execute(manager, client, command['source']))
            finally:
                if 'control_thread' in locals() and control_thread:
                    control_stop.set()
                    signal = ctypes.windll.kernel32.SetEvent
                    signal.argtypes = [ctypes.c_void_p]
                    signal(event_handle)
                    control_thread.join()
                if client is not None:
                    client.stop_channels()
                    client = None
                if manager is not None:
                    previous, manager = manager, None
                    if previous.has_kernel:
                        previous.shutdown_kernel(now=True)
    except Exception as error:
        emit({'event': 'error', 'message': type(error).__name__ + ': ' + str(error)})
    finally:
        if client is not None:
            client.stop_channels()
        if manager is not None and manager.has_kernel:
            manager.shutdown_kernel(now=True)


if __name__ == '__main__':
    main()
