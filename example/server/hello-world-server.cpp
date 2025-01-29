// This file was generated by wayland-scribe v1.0.0
// Source: hello-world.xml

#include "hello-world-server.h"
#include "hello-world-server.hpp"

Wayland::Server::Greeter::Greeter( struct ::wl_client *client, uint32_t id, int version ) {
    m_resource_map.clear();
    init( client, id, version );
}


Wayland::Server::Greeter::Greeter( struct ::wl_display *display, int version ) {
    m_resource_map.clear();
    init( display, version );
}


Wayland::Server::Greeter::Greeter( struct ::wl_resource *resource ) {
    m_resource_map.clear();
    init( resource );
}


Wayland::Server::Greeter::Greeter() {
    m_resource_map.clear();
}


Wayland::Server::Greeter::~Greeter() {
    for (auto it = m_resource_map.begin(); it != m_resource_map.end(); ) {
        Resource *resourcePtr = it->second;

        // Delete the Resource object pointed to by resourcePtr
        resourcePtr->greeterObject = nullptr;
    }

    if ( m_resource ) {
        m_resource->greeterObject = nullptr;
    }

    if ( m_global ) {
        wl_global_destroy( m_global );
        wl_list_remove( &m_displayDestroyedListener.link );
    }
}


void Wayland::Server::Greeter::init( struct ::wl_client *client, uint32_t id, int version ) {
    m_resource = bind( client, id, version );
}


void Wayland::Server::Greeter::init( struct ::wl_resource *resource ) {
    m_resource = bind( resource );
}


Wayland::Server::Greeter::Resource *Wayland::Server::Greeter::add( struct ::wl_client *client, int version ) {
    Resource *resource = bind( client, 0, version );

    m_resource_map.insert( std::pair { client, resource } );
    return resource;
}


Wayland::Server::Greeter::Resource *Wayland::Server::Greeter::add( struct ::wl_client *client, uint32_t id, int version ) {
    Resource *resource = bind( client, id, version );

    m_resource_map.insert( std::pair { client, resource } );
    return resource;
}


void Wayland::Server::Greeter::init( struct ::wl_display *display, int version ) {
    m_global = wl_global_create( display, &::greeter_interface, version, this, bind_func );
    m_displayDestroyedListener.notify = Greeter::display_destroy_func;
    m_displayDestroyedListener.parent = this;
    wl_display_add_destroy_listener( display, &m_displayDestroyedListener );
}


const struct wl_interface *Wayland::Server::Greeter::interface() {
    return &::greeter_interface;
}


Wayland::Server::Greeter::Resource *Wayland::Server::Greeter::allocate() {
    return new Resource;
}


void Wayland::Server::Greeter::bindResource( Resource * ) {
}


void Wayland::Server::Greeter::destroyResource( Resource * ) {
}


void Wayland::Server::Greeter::bind_func( struct ::wl_client *client, void *data, uint32_t version, uint32_t id ) {
    Greeter *that = static_cast<Greeter *>( data );

    that->add( client, id, version );
}


void Wayland::Server::Greeter::display_destroy_func( struct ::wl_listener *listener, void * ) {
    Greeter *that = static_cast<Greeter::DisplayDestroyedListener *>( listener )->parent;

    that->m_global = nullptr;
}


void Wayland::Server::Greeter::destroy_func( struct ::wl_resource *client_resource ) {
    Resource *resource = Resource::fromResource( client_resource );
    Greeter  *that     = resource->greeterObject;

    if ( that ) {
        auto it = that->m_resource_map.begin();
        while ( it != that->m_resource_map.end() ) {
            if ( it->first == resource->client() ) {
                it = that->m_resource_map.erase( it );
            }

            else {
                ++it;
            }
        }
        that->destroyResource( resource );

        that = resource->greeterObject;

        if ( that && ( that->m_resource == resource ) ) {
            that->m_resource = nullptr;
        }
    }

    delete resource;
}


Wayland::Server::Greeter::Resource *Wayland::Server::Greeter::bind( struct ::wl_client *client, uint32_t id, int version ) {
    struct ::wl_resource *handle = wl_resource_create( client, &::greeter_interface, version, id );

    return bind( handle );
}


Wayland::Server::Greeter::Resource *Wayland::Server::Greeter::bind( struct ::wl_resource *handle ) {
    Resource *resource = allocate();

    resource->greeterObject = this;

    wl_resource_set_implementation( handle, &m_greeter_interface, resource, destroy_func );
    resource->handle = handle;
    bindResource( resource );
    return resource;
}


Wayland::Server::Greeter::Resource *Wayland::Server::Greeter::Resource::fromResource( struct ::wl_resource *resource ) {
    if ( !resource ) {
        return nullptr;
    }

    if ( wl_resource_instance_of( resource, &::greeter_interface, &m_greeter_interface ) ) {
        return static_cast<Resource *>( wl_resource_get_user_data( resource ) );
    }

    return nullptr;
}


const struct ::greeter_interface Wayland::Server::Greeter::m_greeter_interface = {
    Wayland::Server::Greeter::handleSayHello
};

void Wayland::Server::Greeter::sayHello( Resource *, const std::string& name ) {
    sendHello( "Aloha, " + name );
}


void Wayland::Server::Greeter::handleSayHello( ::wl_client *, struct wl_resource *resource, const char *name ) {
    Resource *r = Resource::fromResource( resource );

    if ( !r->greeterObject ) {
        return;
    }

    static_cast<Greeter *>( r->greeterObject )->sayHello( r, std::string( name ) );
}


void Wayland::Server::Greeter::sendHello( const std::string& greeting ) {
    if ( !m_resource ) {
        return;
    }

    sendHello(
        m_resource->handle, greeting );
}


void Wayland::Server::Greeter::sendHello( struct ::wl_resource *resource, const std::string& greeting ) {
    greeter_send_hello( resource, greeting.c_str() );
}
