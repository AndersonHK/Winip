# Next output and execution slices

Proposed sequence after 0.2.0. These are designs and acceptance gates, not implemented parity. Each slice should preserve the [rendering contract](Rendering-Contract.md), publish immutable output/layout together, use paired class files, and finish with scripted regressions and a checkpoint commit.

## 1. Native DataFrame tables

Introduce `TableOutput.h/.cpp` for immutable column/index/value data and a native table view. Start with pandas DataFrame headers, index, numbers, strings and nulls. Preserve dtype and original MIME alternatives; do not attempt to reconstruct complete data by scraping a truncated plain-text preview. An IPython formatter can publish a versioned structured MIME representation using pandas' table/split serialization as a reference. pandas is not installed in the selected host interpreter; report that dependency without installing it automatically.

Render visible rows and columns with existing DirectWrite/GPU caches, with complete horizontal/vertical access, selection and copy. Sorting must be explicit and must not mutate the Python object. MultiIndex, long Unicode text, missing values, mixed types and large/wide tables need fixtures. Scoped saved HTML-table fallback and Styler support follow separately; preserve unsupported HTML/styles throughout.

Before large data, extend the current 1 MiB cell-output transport with bounded chunking or owned sidecar files and atomic publication. A native table must not silently truncate values to fit today's transport limit. Measure cold preparation, scroll, resize, cache invalidation and idle; validate offscreen/clipped rows and pixel fidelity at fractional/200% DPI.

## 2. Static and interactive graphs

Implemented in 0.2.0: actual Matplotlib inline PNG output, plus generic PNG/JPEG display, using full-source decode and GPU color/composition. This supports static figures; it does not provide figure pan/zoom, hover, or an interactive Matplotlib canvas.

Next validate labels, legends, multiple figures, transparent backgrounds, explicit retina metadata and large-image navigation. Preserve high-resolution source pixels; don't shrink the source decode to its initial on-screen size. SVG needs a separately validated native vector renderer. Native interactive plots need a defined input/control protocol and ownership for live figure updates.

Plotly/Bokeh are subsequent compatibility targets; neither is installed on this host. Use static exports when the selected environment supplies them. Their browser/JavaScript interaction cannot be described as supported by the static image path. Dashboard architecture remains an explicit lower-priority decision under the no-browser constraint.

## 3. Local Python modules

Implemented and observed: the kernel runs in the notebook's directory. The owner's Downloads notebook imports `modding_tools` with `from modding_tools import *`; that exact import succeeded through `KernelController` using the selected Python. Inspection found ordinary module definitions and standard-library imports. Its tables are custom `Table`/`Row` objects, not pandas DataFrames; future native table support needs an explicit adapter for them.

Only the import and presence of the expected callables were tested. No notebook load-data, mutation or save cell was run, and no mod data was overwritten. The user notebook/module are not copied into this repository.

Next add import diagnostics showing interpreter, working directory and resolved module path, with fixtures for Unicode/spaces and packages in neighboring directories. Restart already gives a fresh import namespace; hot reload should be an explicit operation because existing instances and imports can retain old definitions. Support project search paths only through an explicit setting, not persistent environment changes.

## 4. Explicit parallel work

The frontend already separates UI, rendering, document IO, discovery, settings writes and kernel communication. This is independent of parallel Python computation. The current single kernel preserves ordered cell semantics; Run all must not silently run arbitrary cells concurrently.

Start Python examples with thread pools for IO or native-library work that releases the GIL. For pure Python CPU work, use process pools with importable module functions and explicit inputs/results; Windows spawn cannot reliably import functions defined only in an interactive notebook's `__main__`. The tested Python 3.9 baseline does not provide the newer interpreter-pool API.

A future `ExecutionTask.h/.cpp` / `TaskScheduler.h/.cpp` should own bounded concurrency, cancellation, progress, process jobs and result ordering. Test worker failure, cancellation, oversubscription and shutdown. Coordinate numerical-library thread counts with process counts. Publish complete output snapshots without UI polling while idle.

The owner's modding notebook mutates shared table data sequentially. Parallelize only independent copied datasets or explicitly pure task functions; committing a save remains a distinct user action. Validate deterministic results and file side effects before any scheduling optimization.

## Kernel breadth alongside these slices

Add Run above/below/from selected with explicit inclusion rules, then graceful shutdown, idle-crash detection, stdin, post-idle display updates and stronger output backpressure. Current cooperative interruption preserves a surviving namespace, but native blocking calls may defer the signal. Force stop/restart remain distinct destructive session operations. See [checkpoint evidence](Checkpoint-07.md) and the [reference catalogue](References.md#kernel-images-and-next-slice-design-2026-09-13).
