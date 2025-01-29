// This file was generated by wayland-scribe v1.0.0
// Source: hello-world.xml

#pragma once

#include "wayland-server-core.h"
#include "hello-world-server.h"

#include <iostream>
#include <map>
#include <string>
#include <utility>


namespace Wayland {
    namespace Server {
        class Greeter {
            public:
                Greeter( struct ::wl_client *client, uint32_t id, int version );
                Greeter( struct ::wl_display *display, int version );
                Greeter( struct ::wl_resource *resource );
                Greeter();

                virtual ~Greeter();

                class Resource {
                    public:
                        Resource() : greeterObject( nullptr ), handle( nullptr ) {}
                        virtual ~Resource() {}

                        Greeter *greeterObject;
                        Greeter *object() { return greeterObject; }
                        struct ::wl_resource *handle;

                        struct ::wl_client *client() const { return wl_resource_get_client( handle ); }
                        int version() const { return wl_resource_get_version( handle ); }

                        static Resource *fromResource( struct ::wl_resource *resource );
                };

                void init( struct ::wl_client *client, uint32_t id, int version );
                void init( struct ::wl_display *display, int version );
                void init( struct ::wl_resource *resource );

                Resource *add( struct ::wl_client *client, int version );
                Resource *add( struct ::wl_client *client, uint32_t id, int version );
                Resource *add( struct wl_list *resource_list, struct ::wl_client *client, uint32_t id, int version );

                Resource *resource() { return m_resource; }
                const Resource *resource() const { return m_resource; }

                std::multimap<struct ::wl_client *, Resource *> resourceMap() { return m_resource_map; }
                const std::multimap<struct ::wl_client *, Resource *> resourceMap() const { return m_resource_map; }

                bool isGlobal() const { return m_global != nullptr; }
                bool isResource() const { return m_resource != nullptr; }

                static const struct ::wl_interface *interface();

                static std::string interfaceName() { return interface()->name; }
                static int interfaceVersion() { return interface()->version; }


                void sendHello( const std::string& greeting );
                void sendHello( struct ::wl_resource *resource, const std::string& greeting );

            protected:
                virtual Resource *allocate();

                virtual void bindResource( Resource *resource );
                virtual void destroyResource( Resource *resource );

                virtual void sayHello( Resource *resource, const std::string& name );

            private:
                static void bind_func( struct ::wl_client *client, void *data, uint32_t version, uint32_t id );
                static void destroy_func( struct ::wl_resource *client_resource );
                static void display_destroy_func( struct ::wl_listener *listener, void *data );

                Resource *bind( struct ::wl_client *client, uint32_t id, int version );
                Resource *bind( struct ::wl_resource *handle );

                static const struct ::greeter_interface m_greeter_interface;

                static void handleSayHello( ::wl_client *, struct wl_resource *resource, const char *name );

                std::multimap<struct ::wl_client *, Resource *> m_resource_map;
                Resource *m_resource         = nullptr;
                struct ::wl_global *m_global = nullptr;
                struct DisplayDestroyedListener : ::wl_listener {
                    Greeter *parent;
                };
                DisplayDestroyedListener m_displayDestroyedListener;
        };
    }
}
