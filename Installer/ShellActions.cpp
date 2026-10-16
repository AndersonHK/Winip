#include <windows.h>
#include <msi.h>
#include <shlobj.h>

// Runs after the transaction, including uninstall, from the MSI's embedded DLL.
// Registry ownership remains entirely in the standard MSI tables.
extern "C" __declspec(dllexport) UINT __stdcall RefreshAssociations(MSIHANDLE) {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return ERROR_SUCCESS;
}
