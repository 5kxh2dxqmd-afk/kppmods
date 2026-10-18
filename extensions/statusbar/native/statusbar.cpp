#include "common/json.hpp"
#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <kppmods/kppmods.h>
#include <map>
#include <memory>
#include <openlipc.h>
#include <sstream>
#include <string>
#include <syslog.h>
#include <vector>

namespace {
struct GtkList {
    void *data;
    GtkList *next, *prev;
};
struct Icon {
    std::string id, path, uri;
    void *widget = nullptr;
    unsigned attempts = 0;
    bool dynamic = false;
};
const kppmod_api_v2 *api;
const kppmod_context_v2 *context;
std::map<std::string, std::unique_ptr<Icon>> icons;
LIPC *lipc;
unsigned retrySource;
GtkList *(*listTopLevelWindows)(), *(*listWidgetChildren)(void *);
const char *(*typeName)(void *), *(*widgetName)(void *);
void (*freeWidgetList)(GtkList *), (*unref)(void *), (*destroy)(void *);
int (*isInstanceOf)(void *, unsigned long);
unsigned long (*gtkContainerType)();
unsigned (*idleAdd)(int (*)(void *), void *),
    (*timeoutAdd)(unsigned, int (*)(void *), void *);
decltype(&LipcOpenEx) lipcOpen;
decltype(&LipcRegisterStringProperty) lipcRegister;
decltype(&LipcClose) lipcClose;
decltype(&LipcSetStringProperty) lipcSet;
void *(*pixbufLoad)(const char *, int, int, int, void **), *(*imageNew)(void *),
    *(*eventNew)();
void (*addToContainer)(void *, void *),
    (*packIntoBox)(void *, void *, int, int, unsigned), (*showWidget)(void *);
void (*setWidgetName)(void *, const char *), (*addWidgetEvents)(void *, int);
unsigned long (*connectSignal)(void *, const char *, void *, void *, void *, int);

void logMessage(int level, const std::string &message) {
    api->log(context, level, message.c_str());
}
bool containsBattery(void *widget) {
    const char *name = widgetName(widget);
    if (name && !strcmp(name, "BatteryIcon")) {
        return true;
    }
    if (!isInstanceOf(widget, gtkContainerType())) {
        return false;
    }
    GtkList *list = listWidgetChildren(widget);
    bool found = false;
    for (GtkList *p = list; p && !found; p = p->next) {
        found = containsBattery(p->data);
    }
    freeWidgetList(list);
    return found;
}
void *findStatusBarContainer(void *widget) {
    const char *name = typeName(widget);
    if (name && !strcmp(name, "GtkHBox") && containsBattery(widget)) {
        return widget;
    }
    if (!isInstanceOf(widget, gtkContainerType())) {
        return nullptr;
    }
    GtkList *list = listWidgetChildren(widget);
    void *found = nullptr;
    for (GtkList *p = list; p && !found; p = p->next) {
        found = findStatusBarContainer(p->data);
    }
    freeWidgetList(list);
    return found;
}
int insertPendingIcons(void *);
void scheduleIconInsertion() {
    if (!retrySource) {
        retrySource = timeoutAdd(250, insertPendingIcons, nullptr);
    }
}
void onIconDestroyed(void *, void *data) {
    Icon *icon = static_cast<Icon *>(data);
    icon->widget = nullptr;
    // Explicit removals erase the entry before destroying the widget.
    if (icons.count(icon->id)) {
        icon->attempts = 0;
        scheduleIconInsertion();
    }
}
int onIconPressed(void *, void *, void *data) {
    Icon *icon = static_cast<Icon *>(data);
    if (!icon->uri.empty()) {
        int result = lipcSet(lipc, "com.lab126.appmgrd", "start", icon->uri.c_str());
        logMessage(result ? LOG_ERR : LOG_INFO,
                   icon->id + (result ? ": launch failed" : ": launch requested"));
    }
    return 1;
}
int insertPendingIcons(void *) {
    GtkList *list = listTopLevelWindows();
    void *box = nullptr;
    for (GtkList *p = list; p && !box; p = p->next) {
        box = findStatusBarContainer(p->data);
    }
    freeWidgetList(list);
    bool pending = false;
    for (auto &entry : icons) {
        Icon &icon = *entry.second;
        if (icon.widget || icon.attempts >= 120) {
            continue;
        }
        ++icon.attempts;
        if (!box) {
            if (icon.attempts == 120) {
                logMessage(LOG_ERR, icon.id + ": battery container unavailable");
            } else {
                pending = true;
            }
            continue;
        }
        void *pixbuf = pixbufLoad(icon.path.c_str(), 32, 32, 1, nullptr);
        if (!pixbuf) {
            icon.attempts = 120;
            logMessage(LOG_ERR, icon.id + ": cannot load image " + icon.path);
            continue;
        }
        icon.widget = eventNew();
        void *image = imageNew(pixbuf);
        unref(pixbuf);
        setWidgetName(icon.widget, icon.id.c_str());
        addWidgetEvents(icon.widget, (1 << 8) | (1 << 9));
        connectSignal(icon.widget, "button-release-event",
                      reinterpret_cast<void *>(onIconPressed), &icon, nullptr, 0);
        connectSignal(icon.widget, "destroy", reinterpret_cast<void *>(onIconDestroyed),
                      &icon, nullptr, 0);
        addToContainer(icon.widget, image);
        packIntoBox(box, icon.widget, 0, 0, 6);
        showWidget(icon.widget);
        logMessage(LOG_INFO, icon.id + ": icon installed");
    }
    if (!pending) {
        retrySource = 0;
    }
    return pending ? 1 : 0;
}
struct Request {
    std::string id, path, uri;
    bool remove, dynamic, clear;
};
int apply(void *data) {
    std::unique_ptr<Request> request(static_cast<Request *>(data));
    if (request->clear) {
        for (auto i = icons.begin(); i != icons.end();) {
            if (!i->second->dynamic) {
                ++i;
                continue;
            }
            std::unique_ptr<Icon> item = std::move(i->second);
            i = icons.erase(i);
            if (item->widget) {
                destroy(item->widget);
            }
        }
        return 0;
    }
    auto old = icons.find(request->id);
    if (request->dynamic && old != icons.end() && !old->second->dynamic) {
        logMessage(LOG_ERR,
                   request->id + ": dynamic update conflicts with static item");
        return 0;
    }
    if (old != icons.end()) {
        std::unique_ptr<Icon> item = std::move(old->second);
        icons.erase(old);
        if (item->widget) {
            destroy(item->widget);
        }
    }
    if (!request->remove) {
        std::unique_ptr<Icon> icon(new Icon);
        icon->id = request->id;
        icon->path = request->path;
        icon->uri = request->uri;
        icon->dynamic = request->dynamic;
        const std::string id = icon->id;
        icons.emplace(id, std::move(icon));
        scheduleIconInsertion();
    }
    return 0;
}
int putItem(const char *id, const char *path, const char *uri, bool dynamic) {
    if (!id || !*id || !path || path[0] != '/' || !uri || strpbrk(id, "\t\r\n") ||
        strpbrk(path, "\t\r\n") || strpbrk(uri, "\t\r\n")) {
        return EINVAL;
    }
    std::unique_ptr<Request> request(new Request{id, path, uri, false, dynamic, false});
    if (!idleAdd(apply, request.get())) {
        return EIO;
    }
    request.release();
    return 0;
}
int put(const char *id, const char *path, const char *uri) {
    return putItem(id, path, uri, false);
}
int removeItem(const char *id, bool dynamic) {
    if (!id || !*id || strpbrk(id, "\t\r\n")) {
        return EINVAL;
    }
    std::unique_ptr<Request> request(new Request{id, "", "", true, dynamic, false});
    if (!idleAdd(apply, request.get())) {
        return EIO;
    }
    request.release();
    return 0;
}
int clearDynamic() {
    std::unique_ptr<Request> request(new Request{"", "", "", true, true, true});
    if (!idleAdd(apply, request.get())) {
        return EIO;
    }
    request.release();
    return 0;
}
std::vector<std::string> split(const std::string &line) {
    std::vector<std::string> result;
    size_t start = 0;
    for (;;) {
        size_t end = line.find('\t', start);
        result.push_back(
            line.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) {
            return result;
        }
        start = end + 1;
    }
}
LIPCcode update(LIPC *, const char *, void *value, void *) {
    const char *text = LIPC_SETTER_VTOS(value);
    if (!text || strnlen(text, 8193) > 8192 || strpbrk(text, "\r\n")) {
        return LIPC_ERROR_INVALID_ARG;
    }
    try {
        const auto f = split(text);
        int result = EINVAL;
        if (f.size() == 4 && f[0] == "put") {
            result = putItem(f[1].c_str(), f[2].c_str(), f[3].c_str(), true);
        }
        if (f.size() == 2 && f[0] == "remove") {
            result = removeItem(f[1].c_str(), true);
        }
        if (f.size() == 1 && f[0] == "clear_dynamic") {
            result = clearDynamic();
        }
        return result == 0        ? LIPC_OK
               : result == EINVAL ? LIPC_ERROR_INVALID_ARG
                                  : LIPC_ERROR_INTERNAL;
    } catch (...) {
        return LIPC_ERROR_INTERNAL;
    }
}
} // namespace
extern "C" __attribute__((visibility("default"))) int
kppmod_init_v2(const kppmod_api_v2 *host, const kppmod_context_v2 *hostContext) {
    if (!host || host->abi != KPPMOD_ABI_V2 || host->size < sizeof(kppmod_api_v2) ||
        !hostContext || hostContext->abi != KPPMOD_ABI_V2) {
        return EPROTO;
    }
    api = host;
    context = hostContext;
#define RESOLVE(variable, symbol)                                                      \
    variable = reinterpret_cast<decltype(variable)>(dlsym(RTLD_DEFAULT, symbol));      \
    if (!variable) {                                                                   \
        logMessage(LOG_ERR, std::string("missing API ") + symbol);                     \
        return ENOSYS;                                                                 \
    }
    RESOLVE(listTopLevelWindows, "gtk_window_list_toplevels")
    RESOLVE(listWidgetChildren, "gtk_container_get_children")
    RESOLVE(typeName, "g_type_name_from_instance")
    RESOLVE(widgetName, "gtk_widget_get_name")
    RESOLVE(freeWidgetList, "g_list_free")
    RESOLVE(unref, "g_object_unref")
    RESOLVE(destroy, "gtk_widget_destroy")
    RESOLVE(isInstanceOf, "g_type_check_instance_is_a")
    RESOLVE(gtkContainerType, "gtk_container_get_type")
    RESOLVE(idleAdd, "g_idle_add")
    RESOLVE(timeoutAdd, "g_timeout_add")
    RESOLVE(lipcOpen, "LipcOpenEx")
    RESOLVE(lipcRegister, "LipcRegisterStringProperty")
    RESOLVE(lipcClose, "LipcClose")
    RESOLVE(lipcSet, "LipcSetStringProperty")
    RESOLVE(pixbufLoad, "gdk_pixbuf_new_from_file_at_scale")
    RESOLVE(imageNew, "gtk_image_new_from_pixbuf")
    RESOLVE(eventNew, "gtk_event_box_new")
    RESOLVE(addToContainer, "gtk_container_add")
    RESOLVE(packIntoBox, "gtk_box_pack_start")
    RESOLVE(showWidget, "gtk_widget_show_all")
    RESOLVE(setWidgetName, "gtk_widget_set_name")
    RESOLVE(addWidgetEvents, "gtk_widget_add_events")
    RESOLVE(connectSignal, "g_signal_connect_data")
#undef RESOLVE
    int result = api->claim_hook(context, "juno.statusbar.extensions", nullptr);
    if (result) {
        return result;
    }
    LIPCcode lipcError = LIPC_OK;
    lipc = lipcOpen("org.kppmods.statusbar", &lipcError);
    if (!lipc) {
        return EIO;
    }
    result = lipcRegister(lipc, "update", nullptr, update, nullptr);
    if (result) {
        lipcClose(lipc);
        lipc = nullptr;
        return EIO;
    }
    std::ifstream input(context->config);
    auto configuration =
        km::parseJson(std::string(std::istreambuf_iterator<char>(input), {}));
    for (const auto &entry : km::arrayItems(
             km::field(configuration, "processes", "statusbar"), "statusbar")) {
        auto id = km::get<std::string>(entry, "id");
        result = put(id.c_str(), km::get<std::string>(entry, "icon").c_str(),
                     km::get<std::string>(entry, "uri").c_str());
        if (result) {
            logMessage(LOG_ERR, id + ": invalid statusbar entry");
        }
    }
    return 0;
}
