#include <iostream>                
#include <memory>                  // Smart pointers
#include <arpa/inet.h>             // Funciones de red
#include <sys/socket.h>            // Manejo de sockets
#include <unistd.h>                // Funciones del sistema
#include "libs/json.hpp"           // Biblioteca para manejar JSON

using json = nlohmann::json

// Constructor que inicializa el servidor en un puerto específico
Servidor::Servidor(int puerto) : puerto(puerto), servidorActivo(false) {
    // Crea el socket del servidor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Error al crear el socket del servidor" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Configurar la estructura sockaddr_in para especificar la dirección y puerto del servidor
    address.sin_family = AF_INET;                 
    address.sin_addr.s_addr = INADDR_ANY;         
    address.sin_port = htons(puerto);             

    // Vincular el socket a la dirección y puerto especificados
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Error en la vinculación (bind) del socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Servidor iniciado en el puerto " << puerto << std::endl;
}
