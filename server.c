#define _DEFAULT_SOURCE
#define NUMBER_WORDS 10
#define MAX_WORD_SIZE 50
#define N 10 // max. conexiuni tip client
#define log_file "log.txt"

#include "utilities.h"

typedef struct clientParameters{
    int sockfd;
    pthread_t thread;
}clientParameters;

typedef struct fileStruct
{
    char* words[NUMBER_WORDS][MAX_WORD_SIZE];
    char* filePaths[MAX_FILE_PATH];
    size_t num_paths;

}fileStruct;

pthread_mutex_t operation_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t logFileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t connectionMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t updateListCond = PTHREAD_COND_INITIALIZER; // cond. cu scopul de a actualiza structura de date in urma unei operatii de UPLOAD, DELETE, MOVE sau UPDATE

int clientsConnected = 0;
int operationOccured = 0;


fileStruct* applicationStruct = NULL;
char* uploadedFile = NULL; // cel mai recent uploadat fisier
char* deletedFile = NULL; // cel mai recent sters fisier

// variabile ce ne ajuta sa actualizam structura de date 
char* newLocation_moved = NULL; 
char* lastLocation_moved = NULL;

typedef struct numberShows
{
	char word[MAX_WORD_SIZE];
	int count;
}numberShows;

clientParameters clients[N];

int deleteFileFromStruct(int sockfd, char* path) {
    int status = 0;
    if (deletedFile != NULL) {
        free(deletedFile);
        deletedFile = NULL;
    }

    deletedFile = strdup(path);

    if (deletedFile == NULL) {
        perror("Eroare alocare dinamica");
        status = OUT_OF_MEMORY;
    }

    int stat = remove(path);
    if (stat != 0) {
        perror("Eroare stergere fisier");
        status = FILE_NOT_FOUND;
    }
    return status;
}

int compareShows(const void* a, const void* b) {
    return ((numberShows*)b)->count - ((numberShows*)a)->count;
}

int assignShows(numberShows** ptr, const char* path) {
    FILE* fileptr = fopen(path, "r");
    int count = 0;

    char buffer[MAX_WORD_SIZE];
    while (fscanf(fileptr, "%s", buffer) != EOF) {
        count++;
    }
    fclose(fileptr); // calcul numar cuvinte fisier

    fileptr = fopen(path, "r");
    *ptr = (numberShows*)calloc(count, sizeof(numberShows));
    count = 0;
    while (fscanf(fileptr, "%s", buffer) != EOF) {
        strcpy((*ptr)[count++].word, buffer);
    }
    fclose(fileptr); // asignare cuvinte fisier

    fileptr = fopen(path, "r");
    while (fscanf(fileptr, "%s", buffer) != EOF) {
        for (int i = 0; i < count; i++) {
            if (strcmp((*ptr)[i].word, buffer) == 0) {
                (*ptr)[i].count++;
            }
        }
    } // asignare nr. aparitii fisier
    fclose(fileptr);

    qsort(*ptr, count, sizeof(numberShows), compareShows); // sortare cuvinte in fct. de aparitii folosind regula compareShows

    return count;
}

void addPathInStruct(char* path, fileStruct* applicationStruct) {
        applicationStruct->filePaths[applicationStruct->num_paths] = strdup(path);
        applicationStruct->num_paths++;


       char wordslist[NUMBER_WORDS][MAX_WORD_SIZE];
       
       numberShows* showptr = NULL; // asignam numar aparitii pt fiecare cuvant
       int count = assignShows(&showptr, path);
        
        // in showptr avem sortate descrescator toate cuvintele din path
        // in functie de numarul de aparitii
        // TBD: 
        // incarcare wordslist in campul word din structura unica applicationStruct pe pozitia num_paths-1 (path recent adaugat)
    }



void logOperation(const char *operation, const char *fileAffected, const char *searchedWord)
{
    time_t now = time(NULL);
    struct tm *tm_struct = localtime(&now);

    pthread_mutex_lock(&logFileMutex);

    FILE *file = fopen(log_file, "a");
    if (file == NULL)
    {
        perror("Eroare la deschiderea fisierului de log!");
        pthread_mutex_unlock(&logFileMutex);
        return;
    }

    fprintf(file, "%d-%d-%d %d:%d:%d, %s",
            tm_struct->tm_year + 1900, tm_struct->tm_mon + 1, tm_struct->tm_mday,
            tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec,
            operation);

    if (fileAffected != NULL)
    {
        fprintf(file, ", %s", fileAffected);
    }

    if (searchedWord != NULL)
    {
        fprintf(file, ", %s", searchedWord);
    }

    fprintf(file, "\n");

    fclose(file);
    pthread_mutex_unlock(&logFileMutex);
}



int directoryExists(const char *path) {
    return access(path, F_OK) == 0;
}


fileStruct* initiateFileStruct()
{
    applicationStruct = (fileStruct*)malloc(sizeof(fileStruct));
    if(applicationStruct == NULL)
    {
        perror("Eroare alocare memorie");
        return NULL;
    }

    
    applicationStruct->num_paths = 0;

    return applicationStruct;
}

void* struct_routine() {
    initiateFileStruct();

    while (1) {
        pthread_mutex_lock(&operation_lock);
        while (operationOccured == 0) {
            pthread_cond_wait(&updateListCond, &operation_lock);
        }

        // Operatii specifice threadului dupa operatii de update, delete, move

        if (operationOccured == UPLOAD) {
            addPathInStruct(uploadedFile, applicationStruct);
            free(uploadedFile);
            uploadedFile = NULL;
        }

        if (operationOccured == MOVE) {
            int stat = remove(lastLocation_moved);
            if (stat != 0) {
                perror("Eroare stergere fisier");
                exit(EXIT_FAILURE);
            }
            for (int i = 0; i < applicationStruct->num_paths; ++i) {
                if (strcmp(applicationStruct->filePaths[i], lastLocation_moved) == 0) {
                    free(applicationStruct->filePaths[i]);
                    for (int j = i; j < applicationStruct->num_paths - 1; ++j) {
                        applicationStruct->filePaths[j] = applicationStruct->filePaths[j + 1];
                    }
                    --applicationStruct->num_paths;
                    break;
                }
            }
            addPathInStruct(newLocation_moved, applicationStruct);
        }

        if (operationOccured == DELETE) {
            for (int i = 0; i < applicationStruct->num_paths; ++i) {
                if (strcmp(applicationStruct->filePaths[i], deletedFile) == 0) {
                    free(applicationStruct->filePaths[i]);
                    applicationStruct->filePaths[i] = NULL;
                    for (int j = i; j < applicationStruct->num_paths - 1; ++j) {
                        applicationStruct->filePaths[j] = applicationStruct->filePaths[j + 1];
                    }
                    --applicationStruct->num_paths;
                    free(deletedFile);
                    deletedFile = NULL;
                    break;
                }
            }
        }

        operationOccured = 0;
        pthread_mutex_unlock(&operation_lock);
    }

    return NULL;
}

void signal_action()
{
    printf("Inchidere conexiuni...\n");
    for(int i = 0; i < N; i++)
    {
        if(clients[i].sockfd != 0) // inseamna ca exista clientul asta
        {
            pthread_join(clients[i].thread, NULL);
            close(clients[i].sockfd);
        }
    }
    printf("Conexiuni terminate, inchidere aplicatie.\n");
    exit(0);

}

void *signal_handler()
{
    __sigset_t set;
    int sig;

    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);
    
    while (1)
    {
        sigwait(&set, &sig); // asteptam oricare din semnalele din set.

        if (sig == SIGTERM || sig == SIGINT)
        {
            signal_action();
        }
    }
    return NULL;
}

void executeListOperation(int sockfd)
{
    pthread_mutex_lock(&operation_lock);

        uint32_t numberPaths = applicationStruct->num_paths;

        
        ssize_t bytesSend = send(sockfd, &numberPaths, sizeof(numberPaths), 0);
        if(bytesSend < 0)
        {
            perror("Eroare send LIST!");
            return;
        }
        if(numberPaths != 0)
        {for(int i = 0; i < numberPaths; i++)
            {
                uint32_t pathBytes = strlen(applicationStruct->filePaths[i]);
                bytesSend = send(sockfd, &pathBytes, sizeof(pathBytes), 0);
                if(bytesSend < 0)
                {
                    perror("Eroare send LIST!");
                    return;
                }

                char path[128];
                strcpy(path, applicationStruct->filePaths[i]);

                bytesSend = send(sockfd, path, pathBytes, 0);
                if(bytesSend < 0)
                {
                    perror("Eroare send LIST!");
                    return;
                }
            }
        }

        pthread_mutex_unlock(&operation_lock);
}

void loadUploadParamters(uint32_t* status, int sockfd, uint32_t* pathSize, char** serverpath, char** filename,
  uint32_t* filename_size, uint32_t* fileDimension)
{
        
        ssize_t bytesReceived = recv(sockfd, &(*pathSize), sizeof(*pathSize), 0);
        if(bytesReceived < 0)
        {
            perror("Eroare recv UPLOAD!");
            *status = OTHER_ERROR;
        }
        *serverpath = (char*)calloc(*pathSize+1, sizeof(char));
        if(*serverpath == NULL)
        {
            perror("Eroare alocare dinamica upload!");
            *status = OUT_OF_MEMORY;
        }
        bytesReceived = recv(sockfd, *serverpath, *pathSize, 0); 
        if(bytesReceived < 0)
        {
            perror("Eroare recv UPLOAD!");
            *status = OTHER_ERROR;
        }
         bytesReceived = recv(sockfd, &(*filename_size), sizeof(*filename_size), 0); 
        if(bytesReceived < 0)
        {
            perror("Eroare recv UPLOAD!");
            *status = OTHER_ERROR;
        }

        *filename = (char*)calloc(*filename_size+1, sizeof(char));
          if(*filename == NULL)
        {
            perror("Eroare alocare dinamica upload!");
             *status = OUT_OF_MEMORY;
        }
        bytesReceived = recv(sockfd, *filename, *filename_size, 0); 
        if(bytesReceived < 0)
        {
            perror("Eroare recv UPLOAD!");
            *status = OTHER_ERROR;
        }

        bytesReceived = recv(sockfd, &(*fileDimension), sizeof(*fileDimension), 0);
        if(bytesReceived < 0)
        {
            perror("Eroare recv UPLOAD!");
            *status = OTHER_ERROR;
        }
}



void loadUpdateParameters(uint32_t* status, uint32_t* size, char** path, char** content,
                            uint32_t* startByte, uint32_t* contentSize,  int sockfd)
{
        *path = (char*)calloc(MAX_FILE_PATH, sizeof(char));
        if(*path == NULL)
        {
            perror("Eroare alocare dinamica UPDATE!");
            *status = OUT_OF_MEMORY;
        }

        ssize_t receivedBytes = recv(sockfd, &(*size), sizeof(*size), 0);
        if(receivedBytes <= 0)
        {
            perror("Eroare recv UPDATE");
            free(*path);
            *status = OTHER_ERROR;
        } 

        receivedBytes = recv(sockfd, *path, *size, 0);
        if(receivedBytes <= 0)
        {
            perror("Eroare recv UPDATE");
            free(*path);
            *status = OTHER_ERROR;
        } 

        receivedBytes = recv(sockfd, &(*contentSize), sizeof(*contentSize), 0);
        if(receivedBytes <= 0)
        {
            perror("Eroare recv UPDATE");
            free(*path);
            *status = OTHER_ERROR;
        } 

        *content = (char*)calloc(MAX_FILE_PATH, sizeof(char));
        if(*content == NULL)
        {
            perror("Eroare alocare dinamica UPDATE!");
            free(*path);
            *status = OUT_OF_MEMORY;
        }
        receivedBytes = recv(sockfd, *content, *contentSize, 0);
        if(receivedBytes <= 0)
        {
            perror("Eroare recv UPDATE");
            free(*path);
            *status = OTHER_ERROR;
        } 

        receivedBytes = recv(sockfd, &(*startByte), sizeof(*startByte), 0);
        if(receivedBytes <= 0)
        {
            perror("Eroare recv UPDATE");
            free(*path);
            *status = OTHER_ERROR;
        } 

}

int verifyPath(char* path)
{
    return 1;
}
void loadMoveParameters(uint32_t* status, int sockfd, char** new_path, char** old_path, uint32_t* new_size, uint32_t* old_size)
{
     ssize_t receivedBytes = recv(sockfd, &(*old_size), sizeof(*old_size), 0);
        if(receivedBytes <0)
        {
            perror("Eroare recv MOVE!");
            *status = OTHER_ERROR;
        
        }
        *old_path = (char*)calloc(*old_size+1, sizeof(char));
        if(*old_path == NULL) 
        {
            perror("Eroare alocare MOVE!");
                *status = OUT_OF_MEMORY;
        }
        receivedBytes = recv(sockfd, *old_path, *old_size, 0);
        if(receivedBytes <0)
        {
            perror("Eroare recv MOVE!");
            *status = OTHER_ERROR;
        }
        
        receivedBytes = recv(sockfd, &(*new_size), sizeof(*new_size), 0);
        if(receivedBytes <0)
        {
            perror("Eroare recv MOVE!");
            *status = OTHER_ERROR;
        }
        *new_path = (char*)calloc(*new_size+1, sizeof(char));
        if(*new_path == NULL) 
        {
            perror("Eroare alocare MOVE!");
            *status = OUT_OF_MEMORY;
        }
        receivedBytes = recv(sockfd, *new_path, *new_size, 0);
        if(receivedBytes <0)
        {
            perror("Eroare recv MOVE!");
            *status = OTHER_ERROR;
        }
}
void moveFile(uint32_t* status, char* old_path, char* new_path) {
    FILE* old_file = fopen(old_path, "rb");
    FILE* new_file = fopen(new_path, "wb");

    if (old_file == NULL || new_file == NULL) {
        *status = BAD_ARGUMENTS; // una din cai e cel mai prb. director
        perror("Eroare la deschidere fisiere");
        if (old_file != NULL) fclose(old_file);
        if (new_file != NULL) fclose(new_file);
        return;
    }

    char buffer[SIZE];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), old_file)) > 0) {
        fwrite(buffer, 1, bytesRead, new_file);
    }

    fclose(old_file);
    fclose(new_file);
}
void executeOperation(uint32_t opCode, int sockfd)
{
    uint32_t status = SUCCESS;
    if (opCode == LIST)
    {   
        executeListOperation(sockfd);
    }
    else if (opCode == DOWNLOAD)
    {
        pthread_mutex_lock(&operation_lock);
        uint32_t numberPaths = applicationStruct->num_paths;
        pthread_mutex_unlock(&operation_lock);

        ssize_t sendBytes = send(sockfd, &numberPaths, sizeof(numberPaths),0);
        if(sendBytes <0)
        {
            perror("Eroare send UPDATE!");
            close(sockfd);
           exit(-1);
        }

        executeListOperation(sockfd);

        uint32_t name_size;
        char* name = NULL;

        ssize_t receiveBytes = recv(sockfd, &name_size, sizeof(name_size), 0);
        if(receiveBytes <= 0)
        {
            perror("Eroare receive UPDATE!");
            status = OTHER_ERROR;
        }
        name = (char*)calloc(name_size+1, sizeof(char));
        if(name == NULL)
        {
            perror("Eroare alocare dinamica!");
            status = OUT_OF_MEMORY;
        }

        receiveBytes = recv(sockfd, name, name_size, 0);
        if(receiveBytes <= 0)
        {
            perror("Eroare receive UPDATE!");
            free(name);
            status = OTHER_ERROR;
        }
        
        uint32_t fileSize;
        receiveBytes = recv(sockfd, &fileSize, sizeof(fileSize), 0);
        if (receiveBytes <= 0)
        {
            perror("Eroare recv UPLOAD!");
            
            free(name);
            status = OTHER_ERROR;
        }

        char finalPath[MAX_FILE_PATH];
        strcpy(finalPath, "client-files/");
        strcat(finalPath, name);

        FILE* fileptr = fopen(finalPath, "wb");
        if(fileptr == NULL)
        {
            perror("Eroare in crearea fisierului de upload!\n");
            close(sockfd);
            free(name);
            status = BAD_ARGUMENTS;
        }


        char buffer[SIZE];
        uint32_t totalBytesReceived=0;
        while(totalBytesReceived < fileSize)
        {
            ssize_t bytesReceived = recv(sockfd, buffer, SIZE, 0);
            if(bytesReceived < 0)
            {
                perror("Eroare recv continut fisier!");
                fclose(fileptr);
                free(name);
                status = OTHER_ERROR;
            }   

            fwrite(buffer, 1, bytesReceived, fileptr);
            totalBytesReceived += bytesReceived;
        }
        fclose(fileptr);
        logOperation("DOWNLOAD", finalPath, "NULL");
        free(name);
    }
    else if (opCode == SEARCH)
    {
        uint32_t pathsFound = 0;
        uint32_t size;
       
        ssize_t bytesReceived = recv(sockfd, &size, sizeof(size), 0);
        if(bytesReceived < 0)
        {
            perror("Eroare recv SEARCH!");
            status = OTHER_ERROR;
        }

        char* word = (char*)calloc(size+1, sizeof(char));
        bytesReceived = recv(sockfd, word, size, 0);
        if(bytesReceived < 0)
        {
            perror("Eroare recv SEARCH!");
            status = OTHER_ERROR;
        }

        pthread_mutex_lock(&operation_lock);
        uint32_t numberPaths = applicationStruct->num_paths;
       
        char paths[SIZE][SIZE];
        for(int i = 0; i < numberPaths; i++)
        {
            FILE* fileptr = fopen(applicationStruct->filePaths[i], "r");
            if(fileptr == NULL)
            {
                perror("Eroare deschidere fisier!");
                status = OTHER_ERROR;
            }
            while(!feof(fileptr))
            {
                char buffer[SIZE];
                fscanf(fileptr, "%s", buffer);
                if(strcmp(buffer, word) == 0)
                {
                    printf("se compara %s cu %s\n", word, buffer);
                    fflush(stdout);
                    strcpy(paths[pathsFound++], applicationStruct->filePaths[i]);
                }
            }
            fclose(fileptr);
        }

        ssize_t bytesSend = send(sockfd, &pathsFound, sizeof(pathsFound), 0);
        if(bytesSend < 0)
        {
            perror("Eroare send SEARCH!");
            status = OTHER_ERROR;
        }   
        for(int i = 0; i < pathsFound; i++)
        {
           uint32_t pathSize = strlen(paths[i]);
           bytesSend = send(sockfd, &pathSize, sizeof(pathSize), 0);
            if(bytesSend < 0)
            {
                perror("Eroare send SEARCH!");
                status = OTHER_ERROR;
            }   

            bytesSend = send(sockfd, paths[i], pathSize, 0);
            if(bytesSend < 0)
            {
                perror("Eroare send SEARCH!");
                status = OTHER_ERROR;
            }   
        }
    
        logOperation("SEARCH", "NULL", word);
        free(word);
        pthread_mutex_unlock(&operation_lock);
    }
    else if (opCode == UPDATE)
    {
        pthread_mutex_lock(&operation_lock);
        uint32_t numberPaths = applicationStruct->num_paths;
        pthread_mutex_unlock(&operation_lock);
        
        ssize_t sendBytes = send(sockfd, &numberPaths, sizeof(numberPaths),0);
        if(sendBytes <=0)
        {
            perror("Eroare send UPDATE!");
            status = OTHER_ERROR;
        }

        if(numberPaths != 0)
        {
            executeListOperation(sockfd);

        }

        uint32_t pathSize;
        uint32_t contentSize;
        uint32_t startByte;
  
        char* content = NULL;
        char* path = NULL;
       
        loadUpdateParameters(&status, &pathSize, &path, &content, &startByte, &contentSize, sockfd);

      
        if(verifyPath(path) == 1)
        {
            int fd = open(path, O_RDWR);
            if(fd == -1)
            {
                perror("Eroare deschidere fisier");
                free(path);
                free(content);
                if(status == SUCCESS)
                 status = FILE_NOT_FOUND;
            }

            if(lseek(fd, startByte, SEEK_SET) == -1)
            {
                perror("Eroare lseek");
                free(path);
                free(content);
                if(status == SUCCESS)
                status = OTHER_ERROR;
            }

            if(write(fd, content, contentSize) == -1)
            {
                perror("Eroare update fisier!");
                free(content);
                free(path);
                if(status == SUCCESS)
                 status = OTHER_ERROR;
            }
            logOperation("UPDATE", path, "NULL");
        }


    }
    else if (opCode == MOVE)
    {
        pthread_mutex_lock(&operation_lock);
        uint32_t numberPaths = applicationStruct->num_paths;
        pthread_mutex_unlock(&operation_lock);
        
        ssize_t sendBytes = send(sockfd, &numberPaths, sizeof(numberPaths),0);
        if(sendBytes <0)
        {
            perror("Eroare send MOVE!");
            status = OTHER_ERROR;
        }
        executeListOperation(sockfd);
        
        char* new_path = NULL;
        char* old_path = NULL;
        uint32_t old_size;
        uint32_t new_size;

        loadMoveParameters(&status, sockfd, &new_path, &old_path, &new_size, &old_size);
        // old_path = server-files/folder1/folder2/etc.../fisier ; new_path = folder1/folder2 si serverul concateneaza => server-files/folder1/folder2
        char* finalpath = (char*)calloc(MAX_FILE_PATH, sizeof(char));
        strcpy(finalpath, "server-files/");
        strcat(finalpath, new_path);

        char aux[MAX_FILE_PATH];
        strcpy(aux, finalpath);
        createDirectories(dirname(aux));
    

        moveFile(&status, old_path, finalpath);

        pthread_mutex_lock(&operation_lock);
        if(newLocation_moved != NULL)
            free(newLocation_moved);
        newLocation_moved = finalpath;
        if(lastLocation_moved != NULL)
            free(lastLocation_moved);
        lastLocation_moved = old_path;
        pthread_cond_signal(&updateListCond);
        operationOccured = MOVE;
    

        pthread_mutex_lock(&connectionMutex);
        printf("> Clientul %d a facut move pentru path %s catre path %s.\n", clientsConnected, old_path, new_path);
        free(new_path);
        pthread_mutex_unlock(&connectionMutex);

        logOperation("MOVE", old_path, "NULL");
        pthread_mutex_unlock(&operation_lock);
    }
    else if (opCode == DELETE)
    {
        pthread_mutex_lock(&operation_lock);
        uint32_t numberPaths = applicationStruct->num_paths;
        pthread_mutex_unlock(&operation_lock);
        
        ssize_t sendBytes = send(sockfd, &numberPaths, sizeof(numberPaths),0);
        if(sendBytes <0)
        {
            perror("Eroare send UPDATE!");
            status = OTHER_ERROR;
        }
            executeListOperation(sockfd);
             char* path = NULL;
            uint32_t path_size;

            ssize_t receivedBytes = recv(sockfd, &path_size, sizeof(path_size), 0);
            if(receivedBytes <= 0)
            {
                perror("Eroare receive UPDATE!");
                if(status == SUCCESS)
                    status = OTHER_ERROR;
            }
            path = (char*)calloc(path_size+1, sizeof(char));
            receivedBytes = recv(sockfd, path, path_size, 0);
            if(receivedBytes <= 0)
            {
                perror("Eroare receive UPDATE!");
                free(path);
                if(status == SUCCESS)
                  status = OTHER_ERROR;
            }

            pthread_mutex_lock(&operation_lock);
            int newstatus;
            if((newstatus = deleteFileFromStruct(sockfd, path)) != SUCCESS) 
                status =newstatus;
            pthread_cond_signal(&updateListCond);
            operationOccured = DELETE;
           

            pthread_mutex_lock(&connectionMutex);
            printf("> Clientul %d a sters fisierul %s.\n", clientsConnected, path);
            pthread_mutex_unlock(&connectionMutex);

            logOperation("DELETE", path, "NULL");
            free(path);
            pthread_mutex_unlock(&operation_lock);

       
    }
    else if (opCode == UPLOAD)
    {
        char* serverpath = NULL;
        uint32_t pathSize;
        char* filename = NULL;
        uint32_t filename_size;
        uint32_t fileDimension;
       
        loadUploadParamters(&status, sockfd, &pathSize, &serverpath, &filename,
                            &filename_size, &fileDimension);
    
        // clientul da o cale gen fol1/fol2/fol3, preiau numele din calea
        // din sistem gen /home/student/fisier => fisier
        char newfilepath[128]; 
        strcpy(newfilepath, "server-files/");
        strcat(newfilepath, serverpath);
        
        createDirectories(newfilepath);

        char* namefilepath = (char*)calloc(MAX_FILE_PATH, sizeof(char));
        if(namefilepath == NULL)
        {
            perror("Eroare alocare dinamica!");
            if(status == SUCCESS)
              status = OUT_OF_MEMORY;
        }

        strcpy(namefilepath, newfilepath);
        strcat(namefilepath, "/");
        strcat(namefilepath, filename);

        FILE* fileptr = fopen(namefilepath, "wb");
        if(fileptr == NULL)
        {
            perror("Eroare in crearea fisierului de upload!\n");
            if(status == SUCCESS)
             status = BAD_ARGUMENTS;
        }       

        char buffer[SIZE];
        uint32_t totalBytesReceived=0;
        while(totalBytesReceived < fileDimension)
        {
            ssize_t bytesReceived = recv(sockfd, buffer, SIZE, 0);
            if(bytesReceived < 0)
            {
                perror("Eroare recv continut fisier!");
                fclose(fileptr);
                if(status == SUCCESS)
                    status = OTHER_ERROR;
            }   

            fwrite(buffer, 1, bytesReceived, fileptr);
            totalBytesReceived += bytesReceived;
        }
        fclose(fileptr);

        free(serverpath);
        free(filename);
        
        pthread_mutex_lock(&operation_lock);
        if(uploadedFile != NULL)
          {free(uploadedFile);
            uploadedFile = NULL; }
        uploadedFile = namefilepath;
           
        pthread_cond_signal(&updateListCond); // informam threadul ce manevreaza structura ca s-a efectuat o operatie asupra listei
        operationOccured = UPLOAD;       
  


        pthread_mutex_lock(&connectionMutex);
        printf("> Clientul %d a facut upload la fisierul %s\n", clientsConnected, namefilepath);
        pthread_mutex_unlock(&connectionMutex);

        logOperation("UPLOAD", namefilepath, "NULL");
        pthread_mutex_unlock(&operation_lock);
  
    }
    else
    {
        status = UNKNOWN_OPERATION;
    }

    ssize_t sendBytes = send(sockfd, &status, sizeof(status), 0);
    if(sendBytes < 0)
    {
        perror("Eroare send status");
        close(sockfd);
        exit(-1);
    }

}
void *connection_handler(void *params)
{
    clientParameters* parameters = (clientParameters*)params;
    int fd = parameters->sockfd;

    uint32_t op;
    do
    {
        ssize_t bytesReceived = recv(fd, &op, sizeof(op), 0);
        if (bytesReceived < 0)
        {
            perror("Eroare recv!");
        }
        if (bytesReceived == 0)
            break;
        executeOperation(op, fd);
    } while (op != EXIT);

    free(params);
    close(fd);

    pthread_mutex_lock(&connectionMutex);
    clientsConnected--;
    printf("> Client deconectat. Numar clienti actual: %d\n", clientsConnected);
    pthread_mutex_unlock(&connectionMutex);
    return NULL;
}

void *listen_routine(void *arg)
{
    int sockfd = *(int *)arg;
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    while (1)
    {
     
        int *newsockfd = (int *)malloc(sizeof(int));
        *newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (*newsockfd < 0)
        {
            perror("Eroare in acceptarea conexiunii clientului!");
            free(newsockfd);
            continue;
        }

        pthread_mutex_lock(&connectionMutex);
        uint32_t status;
        if (clientsConnected >= N)
        {
            status = SERVER_BUSY;
            ssize_t bytesSent = send(*newsockfd, &status, sizeof(status), 0);
            if (bytesSent <= 0)
            {
                perror("Eroare send in listen_routine!");
            }
            close(*newsockfd);
            free(newsockfd);
            pthread_mutex_unlock(&connectionMutex);
        }
        else
        { // clientsConnected < N
            clientsConnected++;
             printf("> Client conectat. Numar clienti conectati: %d\n", clientsConnected);
          

            clientParameters* params = (clientParameters*)malloc(sizeof(clientParameters));
            params->sockfd = *newsockfd;

            pthread_mutex_lock(&clients_lock);
            clients[clientsConnected-1] = *params;
            pthread_mutex_unlock(&connectionMutex);
            pthread_t handle_connection_thread;
            
            
            
            if (pthread_create(&handle_connection_thread, NULL, connection_handler, params) != 0)
            {
                perror("Eroare creare thread conexiune client!");
                close(*newsockfd);
                free(newsockfd);
            }
            clients[clientsConnected-1].thread = handle_connection_thread;
            pthread_mutex_unlock(&clients_lock);
            pthread_detach(handle_connection_thread);

            status = SUCCESS;
            ssize_t bytesSent = send(*newsockfd, &status, sizeof(status), 0);
            if (bytesSent <= 0)
            {
                perror("Eroare send in listen_routine!");
            }
        }
    }
}



int main()
{
    for(int i = 0; i < N; i++)
    {
        clients[i].sockfd = 0;
        clients[i].thread = 0;
    }

    printf("<PID: %d> Start executie proces server\n", getpid());



    pthread_t signalsThread;
    pthread_t listThread;
    pthread_t listenThread;
    __sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL); // blocam gestionarea implicita a acestor semnale


    if(!directoryExists(CLIENT_DIRECTORY))
        if(mkdir(CLIENT_DIRECTORY, 0777) == -1)
        {
            perror("Eroare creare director");
            return 1;
        }
    if(!directoryExists(SERVER_DIRECTORY))
        if(mkdir(SERVER_DIRECTORY, 0777) == -1)
        {
            perror("Eroare creare director");
            return 1;
        }

    if (pthread_create(&signalsThread, NULL, &signal_handler, NULL))
    {
        perror("Eroare la crearea thread-ului pentru gestionarea semnalelor");
        return 0;
    }

    if(pthread_create(&listThread, NULL, &struct_routine, NULL))
    {
        perror("Eroare la crearea thread-ului pentru gestionarea structurii de date");
        return 0;
    }

    int sockfd, portnumber;
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Eroare la realizare socket!");
        return 0;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    portnumber = PORT;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portnumber);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {perror("Eroare la bind");
        return 0;
        }
    listen(sockfd, 5);
  
    if (pthread_create(&listenThread, NULL, &listen_routine, &sockfd))
    {
        perror("Eroare lansare thread listen");
        return 0;
    }

    pthread_join(signalsThread, NULL);
    pthread_join(listenThread, NULL);
    pthread_join(listThread, NULL);
    close(sockfd);

    return 0;
}