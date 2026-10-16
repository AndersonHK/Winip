#include "AppSettings.h"
#include "Notebook.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <mutex>
#include <thread>
#include <condition_variable>

struct AppSettings::Impl {
    std::filesystem::path file;
    nlohmann::json values, pending;
    std::wstring error;
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    bool dirty=false, stop=false, invalid=false;
};
AppSettings::AppSettings(const std::filesystem::path& root) : impl(std::make_unique<Impl>()) {
    PWSTR downloads=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_Downloads,0,nullptr,&downloads)))
        throw std::runtime_error("Cannot resolve Downloads folder");
    std::wstring folder(downloads);CoTaskMemFree(downloads);
    impl->file=root/L"settings.json";
    impl->values={{"hdr_enabled",false},{"open_folder_mode","last"},{"fixed_open_folder",Utf8(folder)},{"last_open_folder",Utf8(folder)}};
    if(std::filesystem::exists(impl->file)) {
        try {
            std::ifstream input(impl->file,std::ios::binary);
            auto value=nlohmann::json::parse(input);
            if(!value.is_object())throw std::runtime_error("Settings must be an object");
            value.value("hdr_enabled",false);
            auto mode=value.value("open_folder_mode",std::string("last"));
            if(mode!="last" && mode!="fixed")throw std::runtime_error("Invalid open_folder_mode");
            for(const char* key:{"fixed_open_folder","last_open_folder"}) {
                auto path=Utf16(value.value(key,Utf8(folder)));
                if(!std::filesystem::path(path).is_absolute())throw std::runtime_error("Settings folder must be absolute");
            }
            impl->values.update(value);
        } catch(const std::exception& e) {
            impl->invalid=true;impl->error=L"Settings ignored: "+Utf16(e.what());
        }
    }
    impl->worker=std::thread([this] {
        for(;;) {
            nlohmann::json value;
            {std::unique_lock<std::mutex> lock(impl->mutex);
                impl->wake.wait(lock,[&]{return impl->stop || impl->dirty;});
                if(!impl->dirty)return;
                value=impl->pending;impl->dirty=false;
            }
            try {
                if(impl->invalid) {
                    auto backup=impl->file;backup+=L".invalid";
                    std::filesystem::copy_file(impl->file,backup,std::filesystem::copy_options::overwrite_existing);
                    impl->invalid=false;
                }
                auto temporary=impl->file;temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
                {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
                    output<<value.dump(2)<<"\n";output.close();
                    if(!output)throw std::runtime_error("Cannot write settings");
                }
                if(!MoveFileExW(temporary.c_str(),impl->file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
                    throw std::runtime_error("Cannot replace settings file");
                std::lock_guard<std::mutex> lock(impl->mutex);impl->error.clear();
            } catch(const std::exception& e) {
                std::lock_guard<std::mutex> lock(impl->mutex);impl->error=Utf16(e.what());
            }
        }
    });
    if(!std::filesystem::exists(impl->file))Queue();
}
AppSettings::~AppSettings() {
    {std::lock_guard<std::mutex> lock(impl->mutex);impl->stop=true;}
    impl->wake.notify_one();impl->worker.join();
}
void AppSettings::Queue() {
    std::lock_guard<std::mutex> lock(impl->mutex);impl->pending=impl->values;impl->dirty=true;impl->wake.notify_one();
}
bool AppSettings::HdrEnabled() const {return impl->values["hdr_enabled"].get<bool>();}
bool AppSettings::UseLastFolder() const {return impl->values["open_folder_mode"]=="last";}
std::wstring AppSettings::OpenFolder() const {
    return Utf16(impl->values[UseLastFolder()?"last_open_folder":"fixed_open_folder"].get<std::string>());
}
void AppSettings::SetHdrEnabled(bool enabled) {impl->values["hdr_enabled"]=enabled;Queue();}
void AppSettings::SetUseLastFolder(bool enabled) {impl->values["open_folder_mode"]=enabled?"last":"fixed";Queue();}
void AppSettings::SetFixedFolder(const std::wstring& folder) {
    if(!std::filesystem::path(folder).is_absolute())throw std::invalid_argument("Folder must be absolute");
    impl->values["fixed_open_folder"]=Utf8(folder);Queue();
}
void AppSettings::RememberNotebook(const std::wstring& path) {
    auto folder=std::filesystem::absolute(path).parent_path().wstring();
    impl->values["last_open_folder"]=Utf8(folder);Queue();
}
std::wstring AppSettings::Error() const {std::lock_guard<std::mutex> lock(impl->mutex);return impl->error;}
