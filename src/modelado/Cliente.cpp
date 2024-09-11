#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include "json.hpp" // Incluimos la biblioteca para manejar JSON

using namespace std;
using json = nlohmann::json;

void escucharServidor(int clientSocket) {
    char buffer[1024] = {0};
    
    while (true) {
        ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRecibidos <= 0) {
            cerr << "Conexión cerrada por el servidor o error al recibir." << endl;
            break;
        }

        string mensajeRecibido(buffer, bytesRecibidos);
        json mensajeJson;
        try {
            mensajeJson = json::parse(mensajeRecibido);
        } catch (const json::parse_error& e) {
            cerr << "Error al parsear el mensaje JSON: " << e.what() << endl;
            continue;
        }

        // Verificar si el mensaje es del tipo "PUBLIC_TEXT_FROM"
        if (mensajeJson.contains("type") && mensajeJson["type"] == "PUBLIC_TEXT_FROM") {
            string remitente = mensajeJson["username"];
            string mensaje = mensajeJson["text"];
            cout << remitente << ": " << mensaje << endl;
        }

        memset(buffer, 0, sizeof(buffer));
    }
}

int main() {
    string host;
    int puerto;
    string nombreCliente;

    cout << "Ingresa la dirección del servidor: ";
    cin >> host;
    cout << "Ingresa el puerto: ";
    cin >> puerto;

    // Crear el socket del cliente
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        cerr << "Error al crear el socket del cliente" << endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(puerto);
    inet_pton(AF_INET, host.c_str(), &serverAddress.sin_addr);

    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "Error al conectar con el servidor. Asegúrate de que el servidor está encendido." << endl;
        close(clientSocket);
        return -1;
    }

    cout << "Ingresa tu nombre/ID: ";
    cin.ignore();
    getline(cin, nombreCliente);

    // Crear el mensaje de identificación en formato JSON
    json mensajeIdentificacion = {
        {"type", "IDENTIFY"},
        {"username", nombreCliente}
    };

    // Convertir el mensaje JSON a string y enviarlo al servidor
    string mensajeStr = mensajeIdentificacion.dump();
    send(clientSocket, mensajeStr.c_str(), mensajeStr.length(), 0);

    // Recibir la respuesta del servidor
    char buffer[1024] = {0};
    ssize_t bytesRecibidos = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRecibidos <= 0) {
        cerr << "Error al recibir respuesta del servidor." << endl;
        close(clientSocket);
        return -1;
    }

    // Procesar la respuesta JSON del servidor
    string respuesta(buffer, bytesRecibidos);
    json respuestaJson;
    try {
        respuestaJson = json::parse(respuesta);
    } catch (const json::parse_error& e) {
        cerr << "Error al parsear la respuesta JSON: " << e.what() << endl;
        close(clientSocket);
        return -1;
    }

    // Verificar el resultado de la identificación
    if (respuestaJson.contains("result") && respuestaJson["result"] == "SUCCESS") {
        cout << "Nombre/ID aceptado. Puedes comenzar a enviar mensajes." << endl;
    } else {
        cerr << "Identificación fallida: " << respuestaJson["message"] << endl;
        close(clientSocket);
        return -1;
    }

    // Lanzar un hilo para escuchar los mensajes del servidor
    thread hiloEscuchar(escucharServidor, clientSocket);
    hiloEscuchar.detach();

    // Loop para enviar mensajes al servidor
string mensaje;
while (true) {
    getline(cin, mensaje);

    if (mensaje == "/quit") {
        break;
    }

    // Cambiamos el tipo del mensaje a "PUBLIC_TEXT" para mensajes públicos
    json mensajeJson = {
        {"type", "PUBLIC_TEXT"},
        {"text", mensaje}
    };

    string mensajeStr = mensajeJson.dump();
    send(clientSocket, mensajeStr.c_str(), mensajeStr.length(), 0);
}


    close(clientSocket);
    return 0;
}
