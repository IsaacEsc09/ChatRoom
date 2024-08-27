#include <iostream>
#include <sys/socket.h>

using namespace std;

class Cliente {
private:
    int socketCliente;
    struct sockaddr_in direccionServidor;
    bool conectado;

public:
    Cliente(const std::string& ipServidor, int puerto);
    ~Cliente();

    bool conectar();
    void enviarDatos(const std::vector<uint8_t>& datos);
    std::vector<uint8_t> recibirDatos();
    void desconectar();
};

Cliente::Cliente(const std::string& ipServidor, int puerto) : conectado(false) {
    // Crea el socket
    socketCliente = socket(AF_INET, SOCK_STREAM, 0);
    if (socketCliente < 0) {
        std::cerr << "Error al crear el socket\n";
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(puerto);
    direccionServidor.sin_addr.s_addr = inet_addr(ipServidor.c_str());
}
