#ifndef SERVIDOR_H
#define SERVIDOR_H

#include <map>
#include <thread>
#include <mutex>
#include <string>

class Servidor {
public:
    Servidor(int puerto);
    ~Servidor();

    void iniciar();
    void aceptarConexiones();

private:
    void manejarCliente(int clientSocket);
    bool esPuertoValido(const std::string& puertoStr);

    int puerto_;
    int serverSocket_;
    std::map<std::string, std::thread> clientes_;
    std::mutex coutMutex_;
    std::mutex clientMutex_;
};

#endif
