#ifndef CLIENTE_H
#define CLIENTE_H

#include <string>
#include <thread>
#include <mutex>
#include <netinet/in.h>

class Cliente {
public:
    Cliente(const std::string& host, int puerto);
    ~Cliente();

    void conectar();
    void enviarMensaje(const std::string& mensaje);
    void recibirMensajes();

private:
    std::string host_;
    int puerto_;
    int clientSocket_;
    std::thread hiloRecibir_;
    std::mutex coutMutex_;
};

#endif
