#Realizado por Escobar Gonzalez Isaac Giovani

#Libreria para JSONES
`$ git clone https://github.com/nlohmann/json.git`  
Una vez clonado el repositorio, copia el archivo json.hpp desde el directorio json/single_include/nlohmann/ del repositorio clonado a la carpeta libs/ en tu proyecto.  

`$ mkdir -p libs`$
`$ cp path/to/json/single_include/nlohmann/json.hpp libs/`$


# Compilacion.  
Una vez clonado el repositorio "ChatRoom", debe situarse en el directorio mismo y seguir las siguientes instrucciones para poder ejercutarlo:  
`$ mkdir build`  
`$ cd build`  
`$ cmake ..`  
`$ make`  
Se generarán dos ejecutables: "servidor" y "cliente".  
Para correr/ejecutar el servidor debe estar situado en la carpeta /build y dar el comando:  
`$ ./servidor <puerto>`  
Para el cliente es lo mismo:  
`$ ./cliente`  

#Comandos para el cliente.  
Consultar la lista de usuarios conectados al servidor, con sus respectivos estados:  
`$ /user_list`  
Cambiar el estado:  
`$ /status_ACTIVE`  
`$ /status_AWAY`  
`$ /status_BUSY`  
Desconectarse del servidor:  
`$ /exit`  

#Falta de funcionalidades no implementadas
No se logro implementar Cuartos, Cuartos Privados y los Mensajes privados se envian en forma de JSON
