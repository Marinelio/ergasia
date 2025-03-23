#include <enet/enet.h>
#include <iostream>

using namespace std;
 
int main (int argc, char ** argv) 
{
    if (enet_initialize() != 0) {
        cerr << "ENet initialization failed!" << endl;
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    // Set up server address (listen on all interfaces) and port 25555
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 25555;

    // Create the server host with capacity for 32 clients and 2 channels
    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (!server) {
        cerr << "Server creation failed!" << endl;
        return EXIT_FAILURE;
    }
    cout << "Server started on port " << address.port << endl;


    ENetEvent event;
    while (true) {
        // Process network events with a timeout of 1000 ms
        while (enet_host_service(server, &event, 1000) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                cout << "Client connected from: " 
                     << event.peer->address.host << ":" 
                     << event.peer->address.port << endl;
                event.peer->data = NULL;
            } 
            else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                // Convert packet data to a string
                string message(reinterpret_cast<char*>(event.packet->data));
                cout << "Received: " << message << endl;
                string response;
            
            } 
            else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                cout << "Client disconnected." << endl;
                event.peer->data = NULL;
            }
        }
    }
    


    enet_host_destroy(server);
}