#include "PythonSetupPlan.h"

PythonSetupStage PythonSetupPlan::Stage(const PythonIdentity& identity) {
    if (identity.kernelReady) return PythonSetupStage::Ready;
    if (identity.executable.empty()) return PythonSetupStage::Python;
    if (!identity.error.empty()) return PythonSetupStage::Repair;
    // Missing IPython can also make an installed ipykernel fail to import.
    // Offer the confirmed missing packages first, then validate imports again.
    if (!identity.packageError.empty() && identity.missingPackages.empty()) return PythonSetupStage::Repair;
    if (!identity.pipAvailable) return identity.ensurePipAvailable ? PythonSetupStage::Pip : PythonSetupStage::Repair;
    return PythonSetupStage::Kernel;
}
const wchar_t* PythonSetupPlan::Name(PythonSetupStage stage) {
    switch (stage) {
    case PythonSetupStage::Ready: return L"ready";
    case PythonSetupStage::Python: return L"python";
    case PythonSetupStage::Pip: return L"pip";
    case PythonSetupStage::Kernel: return L"kernel";
    default: return L"repair";
    }
}
const wchar_t* PythonSetupPlan::Title(PythonSetupStage stage) {
    switch (stage) {
    case PythonSetupStage::Ready: return L"Ready to run notebooks";
    case PythonSetupStage::Python: return L"Let's set up Python";
    case PythonSetupStage::Pip: return L"Let's enable pip";
    case PythonSetupStage::Kernel: return L"Jupyter kernel support is missing";
    default: return L"Your Python environment needs attention";
    }
}
const wchar_t* PythonSetupPlan::Description(PythonSetupStage stage) {
    switch (stage) {
    case PythonSetupStage::Ready: return L"A compatible Jupyter kernel is available. No package installation is needed.";
    case PythonSetupStage::Python: return L"Python was not found. We'll guide you through installing Python with pip, then Jupyter kernel support. You can skip setup.";
    case PythonSetupStage::Pip: return L"Python is installed, but pip and Jupyter kernel support are missing. First enable pip, then install the kernel packages.";
    case PythonSetupStage::Kernel: return L"You are missing Jupyter kernel support. Would you like us to install it for you using pip?";
    default: return L"The selected Python cannot load its kernel support. Review the error, choose another interpreter, or follow the official instructions.";
    }
}
const wchar_t* PythonSetupPlan::Choice(PythonSetupStage stage) {
    switch (stage) {
    case PythonSetupStage::Python: return L"Install Python with pip for me (recommended)";
    case PythonSetupStage::Pip: return L"Enable pip in this Python environment (recommended)";
    case PythonSetupStage::Kernel: return L"Install Jupyter kernel support using pip (recommended)";
    default: return L"Guide me through Python setup";
    }
}
bool PythonSetupPlan::CanInstall(PythonSetupStage stage) {
    return stage == PythonSetupStage::Python || stage == PythonSetupStage::Pip || stage == PythonSetupStage::Kernel;
}
