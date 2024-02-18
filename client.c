#include "utilities.h"


typedef struct params
{
    uint32_t opcode;
    int sockfd;
}params;

void printStatus(uint32_t code)
{
       printf("Status operatie: 0x%x\n", code);
}
void executeListOperation(int sockfd)
{
     uint32_t numberPaths;

        ssize_t bytesReceived = recv(sockfd, &numberPaths, sizeof(numberPaths), 0);
        if(bytesReceived < 0)
        {
            perror("Eroare recv LIST!");
            return;
        }

        if(numberPaths != 0)
        {
            printf("> LIST pentru fisierele curente de pe server:\n");
            for(int i = 0; i< numberPaths; i++)
            {
                uint32_t pathBytes;
                bytesReceived = recv(sockfd, &pathBytes, sizeof(pathBytes), 0);
                if(bytesReceived < 0)
                {
                    perror("Eroare recv LIST!");
                    return;
                }

                char* path = (char*)calloc(pathBytes, sizeof(char));
                bytesReceived = recv(sockfd, path, pathBytes, 0);
                if(bytesReceived < 0)
                {
                    perror("Eroare recv LIST!");
                    return;
                }

                printf("%s\n", path);
                free(path);
            }

         

        }
        else
        {
            printf("> Folder pentru fisierele de pe server gol!\n");
        }
}

void* executeOpcode(void* parameters)
{
    params* ptr = (params*)parameters;
    uint32_t opCode = ptr->opcode;
    int sockfd = ptr->sockfd;
    uint32_t status; // plecam de la ideea ca e succes, daca apar probleme pe parcurs modificam variabila
    if (opCode == LIST)
    {
       executeListOperation(sockfd);

    }
    else if (opCode == DOWNLOAD)
    {
        uint32_t numberPaths;
        ssize_t receivedBytes = recv(sockfd, &numberPaths, sizeof(numberPaths), 0);
        if(receivedBytes <= 0)
        {
            perror("Eroare receive UPDATE!");
            return NULL;
        }
        if(numberPaths == 0)
        {
            printf("Pe server nu sunt prezente fisiere momentan!\n");
            return NULL;
        }
        
        executeListOperation(sockfd);

        printf("Pe baza cailor de mai sus tasteaza o cale pe care vrei sa o descarci: ");

        char* path = (char*)calloc(MAX_FILE_PATH, sizeof(char));
        if(path == NULL)
        {
            perror("Eroare alocare dinamica!");
            return NULL;
        }
        
        scanf("%s", path);
        char pathname[MAX_FILE_PATH];
        strcpy(pathname, basename(path));
        uint32_t name_size = strlen(pathname);
        ssize_t sendBytes = send(sockfd, &name_size, sizeof(name_size), 0);
        if(sendBytes <= 0)
        {
            perror("Eroare receive UPDATE!");
            return NULL;
        }

        sendBytes = send(sockfd, pathname, name_size, 0);
        if(sendBytes <= 0)
        {
            perror("Eroare receive UPDATE!");
            return NULL;
        }

        int fd = open(path, O_RDONLY);
        if(fd < 0)
        {
            perror("Eroare deschidere fisier");
            exit(-1);
        }

        struct stat fileStat;
        fstat(fd, &fileStat);
        uint32_t fileSize = fileStat.st_size; 
        sendBytes = send(sockfd, &fileSize, sizeof(fileSize), 0);
        if (sendBytes <= 0)
        {
            perror("Eroare send UPLOAD!");
            close(sockfd);
            return NULL;
        }
        sendfile(sockfd, fd, NULL, fileSize);
        close(fd);

    }
    else if (opCode == SEARCH)
    {
        printf("Tasteaza cuvantul pe care vrei sa il cauti in fisiere: ");

        char* word = (char*)calloc(SIZE+1, sizeof(char));
        scanf("%s", word);

        uint32_t size = strlen(word);
        ssize_t bytesSend = send(sockfd, &size, sizeof(size), 0);
        if(bytesSend < 0)
        {
            perror("Eroare send SEARCH!");
            status = OTHER_ERROR;
        }
        bytesSend = send(sockfd, word, size, 0);
        if(bytesSend < 0)
        {
            perror("Eroare send SEARCH!");
            status = OTHER_ERROR;
        }
     


        uint32_t numberPaths;
        ssize_t recvBytes = recv(sockfd, &numberPaths, sizeof(numberPaths), 0);
        if(recvBytes < 0)
        {
            perror("Eroare recv SEARCH!");
            status = OTHER_ERROR;
        }
        printf("Cuvant %s gasit in urmatoarele fisiere: \n", word);
        free(word);
        for(int i = 0 ; i< numberPaths; i++)
        {
            uint32_t pathSize;
            recvBytes = recv(sockfd, &pathSize, sizeof(pathSize), 0);
            if(recvBytes < 0)
            {
                perror("Eroare recv SEARCH!");
                status = OTHER_ERROR;
            }
            char* buffer = (char*)calloc(pathSize+1, sizeof(char));
            recvBytes = recv(sockfd, buffer, pathSize, 0);
            if(recvBytes < 0)
            {
                perror("Eroare recv SEARCH!");
                status = OTHER_ERROR;
            }
            printf("%s\n", buffer);
            free(buffer);
           
        }
}

    else if (opCode == UPDATE)
    {
        uint32_t numberPaths;
        ssize_t receivedBytes = recv(sockfd, &numberPaths, sizeof(numberPaths), 0);
        if(receivedBytes <= 0)
        {
            perror("Eroare receive UPDATE!");
            return NULL;
        }
        if(numberPaths == 0)
        {
            printf("Pe server nu sunt prezente fisiere momentan!\n");
            return NULL;
        }
        
        executeListOperation(sockfd);
        printf("Tasteaza calea unui fisier de mai sus caruia vrei sa ii faci update: ");

        char* path = (char*)calloc(MAX_FILE_PATH, sizeof(char));
        if(path == NULL)
        {
            perror("Eroare alocare dinamica UPDATE");
            return NULL;
        }
        scanf("%s", path);

        uint32_t size = strlen(path);
        ssize_t bytesSend = send(sockfd, &size, sizeof(size), 0);
        if(bytesSend <= 0)
        {
            perror("Eroare send UPDATE!");
            free(path);
            return NULL;
        }

        bytesSend = send(sockfd, path, size, 0);
        if(bytesSend <= 0)
        {
            perror("Eroare send UPDATE!");
            free(path);
            return NULL;
        }
        free(path);

        char* content = (char*)calloc(MAX_FILE_PATH, sizeof(char));
        printf("> Introdu continutul pe care vrei sa il introduci in acest fisier (MAX 128 octeti)\n ");

        scanf("%s", content);

        uint32_t contentSize = strlen(content);
        bytesSend = send(sockfd, &contentSize, sizeof(contentSize), 0);
        if(bytesSend <= 0)
        {
            perror("Eroare send UPDATE!");
            free(content);
            return NULL;
        }

        bytesSend = send(sockfd, content, contentSize, 0);
        if(bytesSend <= 0)
        {
            perror("Eroare send UPDATE!");
            free(content);
            return NULL;
        }
        free(content);
        
        printf("> Introdu numarul octetului de unde doresti sa plasezi continut modificat: ");
        uint32_t startByte;
        scanf("%ld", &startByte);

        bytesSend = send(sockfd, &startByte, sizeof(startByte), 0);
        if(bytesSend <= 0)
        {
            perror("Eroare send UPDATE!");
            return NULL;
        }

    }
    else if (opCode == MOVE)
    {
        uint32_t numberPaths;
        ssize_t receivedBytes = recv(sockfd, &numberPaths, sizeof(numberPaths), 0);
        if(receivedBytes <= 0)
        {
            perror("Eroare receive MOVE!");
            return NULL;
        }
        if(numberPaths != 0)
       {
            executeListOperation(sockfd);
            char* old_path = NULL;
            char* new_path = NULL;
            uint32_t old_size;
            uint32_t new_size;
            printf("Introdu calea unui fisier pe care vrei sa il muti (din cele de mai sus): ");

            old_path = (char*)calloc(MAX_FILE_PATH, sizeof(char));
            if(old_path == NULL)
            {
                perror("Eroare alocare dinamica MOVE");
                return NULL;
            }
            scanf("%s", old_path);

            old_size = strlen(old_path);

            ssize_t sendBytes = send(sockfd, &old_size, sizeof(old_size), 0);
            if(sendBytes < 0)
            {
                perror("Eroare send MOVE!");
                return NULL;
            }

            sendBytes = send(sockfd, old_path, old_size, 0);
            if(sendBytes < 0)
            {
                perror("Eroare send MOVE!");
                return NULL;
            }

            printf("Introdu calea unde doresti sa muti fisierul: ");

            new_path = (char*)calloc(MAX_FILE_PATH, sizeof(char));
            if(new_path == NULL)
            {
                perror("Eroare alocare dinamica MOVE");
                return NULL;
            }
            scanf("%s", new_path);

            new_size = strlen(new_path);

            sendBytes = send(sockfd, &new_size, sizeof(new_size), 0);
            if(sendBytes < 0)
            {
                perror("Eroare send MOVE!");
                return NULL;
            }

            sendBytes = send(sockfd, new_path, new_size, 0);
            if(sendBytes < 0)
            {
                perror("Eroare send MOVE!");
                return NULL;
            }
            free(old_path);
            free(new_path);
       }
    }
    else if (opCode == DELETE)
    {
        uint32_t numberPaths;
        ssize_t receivedBytes = recv(sockfd, &numberPaths, sizeof(numberPaths), 0);
        if(receivedBytes <= 0)
        {
            perror("Eroare receive UPDATE!");
            return NULL;
        }
        if(numberPaths != 0)
       {
            executeListOperation(sockfd);

            printf("> Introdu calea fisierului de mai sus pe care vrei sa il stergi: ");
            char* filename = (char*)calloc(MAX_FILE_PATH, sizeof(char));
            if(filename == NULL)
            {
                perror("Eroare alocare dinamica!");
                return NULL;
            }

            scanf("%s", filename);

            uint32_t size = strlen(filename);
            ssize_t sendBytes = send(sockfd, &size, sizeof(size), 0);
            if(sendBytes < 0 )
            {
                perror("Eroare send DELETE!");
                return NULL;
            }

            sendBytes = send(sockfd, filename, size, 0);
            if(sendBytes < 0 )
            {
                perror("Eroare send DELETE!");
                return NULL;
            }
      }
      else
      {
        printf("> Pe server nu sunt fisiere momentan!\n");
        status = OTHER_ERROR;
        printStatus(status);
        return NULL;
      }
    

    }
    else if (opCode == UPLOAD) // + nr octeti cale, cale, nr octeti continut fisier, continut fisier
    {

        char serverpath[128];
        char filepath[128];
        printf("> Introdu calea unde doresti sa fie plasat fisierul: ");
        scanf("%s", serverpath);


        uint32_t pathSize = strlen(serverpath);
        ssize_t bytesSend = send(sockfd, &pathSize, sizeof(pathSize), 0);
        if (bytesSend <= 0)
        {
            perror("Eroare send UPLOAD!");
            close(sockfd);
            return NULL;
        }


        bytesSend = send(sockfd, serverpath, pathSize, 0);
        if (bytesSend <= 0)
        {
            perror("Eroare send UPLOAD!");
            close(sockfd);
            return NULL;
        }

        

        printf("> Introdu calea fisierului pe care doresti sa il incarci: ");
        scanf("%s", filepath);

        char filename[32];
        strcpy(filename, basename(filepath));

        uint32_t filename_size = strlen(filename);
        bytesSend = send(sockfd, &filename_size, sizeof(filename_size), 0);
        if (bytesSend <= 0)
        {
            perror("Eroare send UPLOAD!");
            close(sockfd);
            return NULL;
        }

        bytesSend = send(sockfd, filename, filename_size, 0);
        if (bytesSend <= 0)
        {
            perror("Eroare send UPLOAD!");
            close(sockfd);
            return NULL;
        }


        int fd = open(filepath, O_RDONLY);
        if(fd < 0)
        {
            perror("Eroare deschidere fisier");
            exit(-1);
        }

   


        struct stat fileStat;
        fstat(fd, &fileStat);
        uint32_t fileSize = fileStat.st_size; 
        bytesSend = send(sockfd, &fileSize, sizeof(fileSize), 0);
        if (bytesSend <= 0)
        {
            perror("Eroare send UPLOAD!");
            close(sockfd);
            return NULL;
        }
        sendfile(sockfd, fd, NULL, fileSize);
        close(fd);


    }
    
        ssize_t receivedBytes= recv(sockfd, &status, sizeof(status), 0);
        if(receivedBytes < 0)
        {
            perror("Eroare receive UPDATE!");
            return NULL;
        }

        printStatus(status);

    return NULL;
}



int main()
{
    printf("<%d> Start executie client\n", getpid());
    int sockfd;
    struct sockaddr_in addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Eroare la realizare socket!");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    // convertire adresa localhost in forma specifica retelei
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0)
    {
        perror("Eroare inet_pton!");
        exit(1);
    }

    int status = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (status < 0)
    {
        perror("Eroare initializare conectare!");
        exit(1);
    }

    uint32_t code; // server busy sau nu
    ssize_t bytesRecv = recv(sockfd, &code, sizeof(code), 0);
    if (bytesRecv <= 0)
    {
        perror("Eroare recv status server!");
        exit(1);
    }

    if (code == SERVER_BUSY)
    {
        printf("S-a atins limita de clienti conectati. Conexiune refuzata!\n Status: %d", SERVER_BUSY);
        exit(1);
    }

    while (1)
    {
        printf("> Introdu cod operatie: 0x");
        pthread_t execute_code_thread;

        uint32_t op;
        scanf("%x", &op);
        ssize_t bytesSend = send(sockfd, &op, sizeof(op), 0);
        if (bytesSend < 0)
        {
            perror("Eroare send!");
            close(sockfd);
            exit(1);
        }


        params parameters;
        parameters.opcode = op;
        parameters.sockfd = sockfd;
        if (pthread_create(&execute_code_thread, NULL, &executeOpcode, &parameters))
            {
                perror("Eroare creare thread conexiune client!");
                close(sockfd);
                exit(-1);
            }

 
        
        if (pthread_join(execute_code_thread, NULL)) {
            perror("Eroare asteptare thread conexiune client!");
            close(sockfd);
            exit(-1);
        }
    }

    close(sockfd);
    return 0;
}