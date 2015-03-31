/*
 *  This file is part of ocland, a free cloud OpenCL interface.
 *  Copyright (C) 2015  Jose Luis Cercos Pita <jl.cercos@upm.es>
 *
 *  ocland is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ocland is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with ocland.  If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 * @brief ICD cl_platform_id implementation
 * @see platform_id.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#ifdef WIN32
    #ifndef getline
        #include <errno.h>
        /// @note not all functionality is implemented, this is just a simple gets wrapper for now
        int getline(char **lineptr, size_t *n, FILE *stream)
        {
            char buf[256] = { 0 }; //must be enouth for server address
            size_t string_size = 0;
            if ((NULL == lineptr) || (NULL == n) || (NULL == stream) || ferror(stream)) {
                errno = EINVAL;
                return -1;
            }
            if (feof(stream)) {
                return -1;
            }

            char * ret = fgets(buf, sizeof(buf), stream);
            if (NULL == ret) {
                // end of file
                return -1;
            }
            string_size = strlen(buf) + 1;

            if ((NULL == *lineptr) || (string_size > *n)){
                char *newMem = realloc(*lineptr, string_size);
                if (!newMem) {
                    errno = ENOMEM;
                    return -1;
                }
                *lineptr = newMem;
            }
            *n = string_size;
            memcpy(*lineptr, buf, string_size);
            return string_size - 1;
        }
    #endif
#endif

#include <ocland/common/sockets.h>
#include <ocland/client/commands_enum.h>
#include <ocland/common/verbose.h>
#include <ocland/client/platform_id.h>
#include <ocland/common/dataExchange.h>

#ifndef OCLAND_PORT
    /// Default ocland port when it is not specified in the server
    #define OCLAND_PORT 51000u
#endif

/// Number of servers
static cl_uint num_servers = 0;
/// Servers data storage
static oclandServer* servers = NULL;
/// Servers initialization flag
static cl_bool initialized = CL_FALSE;

/** @brief Read servers file "ocland", where IP address of each available server
 * (one per line) are specified.
 *
 * During this function servers list will be generated, but connections will not
 * be established.
 * @return Number of servers.
 * @note The empty lines in the file are ignored.
 * @warning This function is not checking if the servers list has been already
 * set, such that it will allocate new memory, leaking the previous one, and
 * breaking previous references and connections.
 */
cl_uint initLoadServers()
{
    cl_uint i;

    // Load servers definition files
    num_servers = 0;
    FILE *fin = NULL;
    fin = fopen("ocland", "r");
    if(!fin){
        // File does not exist, ocland should return 0 platforms
        VERBOSE("Failure reading \"ocland\": %s\n", strerror(errno));
        return num_servers;
    }
    // Count the number of servers
    char *line = NULL;
    size_t linelen = 0;
    ssize_t read = getline(&line, &linelen, fin);
    while(read != -1) {
        if(strcmp(line, "\n"))
            num_servers++;
        free(line); line = NULL; linelen = 0;
        read = getline(&line, &linelen, fin);
    }
    // Setup the list of servers
    servers = (oclandServer*)malloc(num_servers * sizeof(oclandServer));
    if(!servers){
        // Something wrong & strange happened, abort ocland
        VERBOSE("Failure allocating memory for the servers!\n");
        return num_servers;
    }

    // Setup each server
    rewind(fin);
    i = 0;
    line = NULL; linelen = 0;
    while((read = getline(&line, &linelen, fin)) != -1) {
        if(!strcmp(line, "\n")){
            free(line); line = NULL; linelen = 0;
            continue;
        }

        servers[i] = (oclandServer)malloc(sizeof(struct oclandServer_st));
        servers[i]->address = (char*)malloc((strlen(line) + 1) * sizeof(char));
        servers[i]->socket = (int*)malloc(sizeof(int));
        servers[i]->callbacks_socket = (int*)malloc(sizeof(int));
        servers[i]->callbacks_stream = NULL;

        strcpy(servers[i]->address, line);
        // We don't want the line break
        char * lbreak = strstr(servers[i]->address, "\n");
        if (lbreak)
        {
            *lbreak = '\0';
        }

        *(servers[i]->socket) = -1;
        *(servers[i]->callbacks_socket) = -1;
        free(line);
        line = NULL;
        linelen = 0;
        i++;
    }

    return num_servers;
}

/** @brief Connect to servers.
 * @return Number of active servers.
 * @see initLoadServers
 * @warning This function is not checking if we already connected with the
 * server, and therefore will try to connect again (it may fails).
 */
cl_uint initConnectServers()
{
    int flag, switch_on=1;
    cl_uint i, j, n=0;
    int sin_family = AF_INET6;  // IPv6 by default

#ifdef WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        VERBOSE("Winsock DLL initialization failed with error %d\n", err);
        return 0;
    }
#endif
    for(i = 0; i < num_servers; i++){
        // Try to connect to server
        VERBOSE("Connecting with \"%s\"...\n", servers[i]->address);

        char *address=NULL, *port=NULL;
        address = (char*)malloc(
            (strlen(servers[i]->address) + 1) * sizeof(char));
        if(!address){
            VERBOSE("Failure allocating memory to copy address!\n");
            continue;
        }
        strcpy(address, servers[i]->address);

        // Try to deal with address considering IPv6. In ocland the address
        // should be enclosed by square brackets:
        // http://en.wikipedia.org/wiki/IPv6_address#Literal_IPv6_addresses_in_network_resource_identifiers
        if(strchr(address, '[')){
            port = (char*)malloc(
                strlen(strchr(strchr(address, ']'), ':')) * sizeof(char));
            if(!port){
                VERBOSE("Failure allocating memory to copy the port!\n");
                continue;
            }
            strcpy(port, strchr(strchr(address, ']'), ':') + 1);
            strcpy(strchr(strchr(address, ']'), ':'), "");
        }
        // It is IPv4, so try to find the port
        else{
            sin_family = AF_INET;
            if(strchr(address, ':')){
                port = (char*)malloc(
                    strlen(strchr(address, ':')) * sizeof(char));
                if(!port){
                    VERBOSE("Failure allocating memory to copy the port!\n");
                    continue;
                }
                strcpy(port, strchr(address, ':') + 1);
                strcpy(strchr(address, ':'), "");
            }
        }
        unsigned int port_number = OCLAND_PORT;
        if(port){
            port_number = (unsigned int)strtol(port, NULL, 10);
        }

        // Connect with the server by several ports (in order to get several
        // data streams)
        unsigned int ports[2] = {port_number, port_number + 1};
        int *sockets[2] = {servers[i]->socket, servers[i]->callbacks_socket};
        struct sockaddr_in serv_addrs[2];
        int sockets_failed = 0;
        for(j = 0; j < 2; j++){
            int sockfd = 0;
            if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                VERBOSE("Failure registering the new socket!\n");
                *(servers[i]->socket) = -1;
                sockets_failed = 1;
                break;
            }

            memset(&(serv_addrs[j]), '0', sizeof(struct sockaddr_in));
            serv_addrs[j].sin_family = sin_family;
            serv_addrs[j].sin_port = htons(ports[j]);

            flag = inet_pton(sin_family, address, &serv_addrs[j].sin_addr);
            if(!flag){
                VERBOSE("Invalid server address \"%s\"!\n", address);
                if(sin_family == AF_INET){
                    VERBOSE("\tInterpreting it as IPv4\n");
                }
                else if(sin_family == AF_INET6){
                    VERBOSE("\tInterpreting it as IPv6\n");
                }
                sockets_failed = 1;
                break;
            }
            else if(flag < 0){
                VERBOSE("Invalid address family!\n");
                if(sin_family == AF_INET){
                    VERBOSE("\tAF_INET\n");
                }
                else if(sin_family == AF_INET6){
                    VERBOSE("\tAF_INET6\n");
                }
                else{
                    VERBOSE("\t%d\n", serv_addrs[j].sin_family);
                }
                sockets_failed = 1;
                break;
            }
            if(connect(sockfd, (struct sockaddr *)&serv_addrs[j], sizeof(serv_addrs[j])) < 0){
                VERBOSE("Failure connecting in port %u: %s\n",
                        ports[j],
                        strerror(errno));
                // Connection failed, not a valid server
                sockets_failed = 1;
                break;
            }
            VERBOSE("\tConnected with \"%s\" in port %u!\n",
                    servers[i]->address,
                    ports[j]);
            flag = setsockopt(sockfd,
                              IPPROTO_TCP,
                              TCP_NODELAY,
                              (const void *) &switch_on,
                              sizeof(int));
            if(flag){
                VERBOSE("Failure enabling TCP_NODELAY: %s\n", strerror(errno));
                VERBOSE("\tThe connecting is still considered valid\n");
            }
#ifndef WIN32
            flag = setsockopt(sockfd,
                              IPPROTO_TCP,
                              TCP_QUICKACK,
                              (const void *) &switch_on,
                              sizeof(int));
            if(flag){
                VERBOSE("Failure enabling TCP_QUICKACK: %s\n", strerror(errno));
                VERBOSE("\tThe connecting is still considered valid\n");
            }
#endif
            // Store socket
            *(sockets[j]) = sockfd;
        }
        if (!sockets_failed) {
            n++;
        }
    }
    return n;
}

const char* oclandServerAddress(int socket)
{
    unsigned int i;
    for(i = 0; i < num_servers; i++){
        if(*(servers[i]->socket) == socket){
            return (const char*)(servers[i]->address);
        }
    }
    return NULL;
}

/** @brief load servers file and connect to them.
 * @return Number of active servers.
 * @see initLoadServers()
 * @see initConnectServers()
 */
unsigned int oclandInitServers()
{
    unsigned int i,n;
    if(initialized){
        n = 0;
        for(i = 0; i < num_servers; i++){
            if(*(servers[i]->socket) >= 0)
                n++;
        }
        return n;
    }
    initialized = CL_TRUE;
    n = initLoadServers();
    return initConnectServers();
}

download_stream createCallbackStream(oclandServer server)
{
    if(!server){
        return NULL;
    }
    if((*server->callbacks_socket) < 0){
        return NULL;
    }

    if(getCallbackStream(server)){
        retainCallbackStream(server);
        return getCallbackStream(server);
    }

    download_stream stream = createDownloadStream(server->callbacks_socket);
    if(!stream){
        return NULL;
    }
    server->callbacks_stream = stream;

    return stream;
}

download_stream getCallbackStream(oclandServer server)
{
    if(!server){
        return NULL;
    }
    return server->callbacks_stream;
}

cl_int retainCallbackStream(oclandServer server)
{
    if(!getCallbackStream(server)){
        return CL_INVALID_VALUE;
    }
    return retainDownloadStream(getCallbackStream(server));
}

cl_int releaseCallbackStream(oclandServer server)
{
    cl_int flag;
    download_stream stream = getCallbackStream(server);
    if(!stream){
        return CL_INVALID_VALUE;
    }
    cl_uint rcount = stream->rcount;

    flag = releaseDownloadStream(stream);
    if(flag != CL_SUCCESS){
        return flag;
    }

    if(rcount == 1){
        // The object has been destroyed (stream->rcount = rcount - 1)
        server->callbacks_stream = NULL;
    }

    return CL_SUCCESS;
}

/*
 * Platforms stuff
 * ===============
 */
/// Number of known platforms
cl_uint num_global_platforms = 0;
/// List of known platforms
cl_platform_id *global_platforms = NULL;

int hasPlatform(cl_platform_id platform){
    cl_uint i;
    for(i = 0; i < num_global_platforms; i++){
        if(platform == global_platforms[i])
            return 1;
    }
    return 0;
}

/** @brief Get the platform index in the master list
 * @param platform Platform to look for
 * @return Index of the platform, num_global_platforms if it is not found.
 */
cl_uint platformIndex(cl_platform_id platform)
{
    cl_uint i;
    for(i = 0; i < num_global_platforms; i++){
        if(platform == global_platforms[i])
            break;
    }
    return i;
}

cl_platform_id platformFromServer(pointer srv_platform)
{
    cl_uint i;
    for(i = 0; i < num_global_platforms; i++){
        if(srv_platform == global_platforms[i]->ptr_on_peer)
            return global_platforms[i];
    }
    return NULL;
}

cl_int discardPlatform(cl_platform_id platform)
{
    if(!hasPlatform(platform)){
        return CL_INVALID_VALUE;
    }
    cl_uint i, index=platformIndex(platform);
    free(global_platforms[index]);
    assert(num_global_platforms > 0);
    for(i = index; i < num_global_platforms - 1; i++){
        global_platforms[i] = global_platforms[i + 1];
    }
    num_global_platforms--;
    global_platforms[num_global_platforms] = NULL;
    return CL_SUCCESS;
}

/** @brief Initializes ocland.
 *
 * This function is calling to oclandInitServers() and later is looking for
 * the platforms into connected servers.
 * @return CL_SUCCESS if the platforms are already generated, an error code
 * otherwise.
 * @see oclandInitServers()
 */
cl_int initPlatforms(struct _cl_icd_dispatch *dispatch)
{
    if(initialized){
        return CL_SUCCESS;
    }
    if(!oclandInitServers()){
        // No available servers
        return CL_SUCCESS;
    }

    unsigned int i, j;
    cl_int flag = CL_OUT_OF_HOST_MEMORY;
    int socket_flag = 0;
    unsigned int comm = ocland_clGetPlatformIDs;

    // Count the platforms available
    for(i = 0; i < num_servers; i++){
        // Ensure that we rightly connected with the server
        if(*(servers[i]->socket) < 0)
            continue;
        int *sockfd = servers[i]->socket;
        // Ask for the number of available platforms
        cl_uint num_entries=0, num_platforms;
        socket_flag |= Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
        socket_flag |= Send(sockfd, &num_entries, sizeof(cl_uint), 0);
        socket_flag |= Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
        if((socket_flag) || (flag != CL_SUCCESS)){
            // Something has gone wrong with the server, abort ocland
            return flag;
        }
        socket_flag |= Recv(sockfd, &num_platforms, sizeof(cl_uint), MSG_WAITALL);
        if(socket_flag){
            // The server is KO, abort ocland
            return CL_OUT_OF_HOST_MEMORY;
        }
        num_global_platforms += num_platforms;
    }

    // Setup the array of platforms
    global_platforms = (cl_platform_id*)malloc(
        num_global_platforms * sizeof(cl_platform_id));
    if(!global_platforms){
        return CL_OUT_OF_HOST_MEMORY;
    }

    // Receive the platform pointers from the server
    cl_uint stored_platforms=0;
    for(i = 0; i < num_servers; i++){
        // Ensure that we rightly connected with the server
        if(*(servers[i]->socket) < 0)
            continue;
        int *sockfd = servers[i]->socket;
        // Ask for the number of available platforms
        cl_uint num_entries=num_global_platforms, num_platforms;
        socket_flag |= Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
        socket_flag |= Send(sockfd, &num_entries, sizeof(cl_uint), 0);
        socket_flag |= Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
        if((socket_flag) || (flag != CL_SUCCESS)){
            // Something has gone wrong with the server, abort ocland
            free(global_platforms); global_platforms=NULL;
            return flag;
        }
        socket_flag |= Recv(sockfd, &num_platforms, sizeof(cl_uint), MSG_WAITALL);
        if(socket_flag){
            // The server is KO, abort ocland
            free(global_platforms); global_platforms=NULL;
            return CL_OUT_OF_HOST_MEMORY;
        }
        if(stored_platforms + num_platforms > num_global_platforms){
            // The server is crazy, abort ocland
            free(global_platforms); global_platforms=NULL;
            return CL_OUT_OF_HOST_MEMORY;
        }
        // Ask for the platform pointers
        pointer *server_platforms = (pointer*)malloc(
            num_platforms * sizeof(pointer));
        if(!server_platforms){
            free(global_platforms); global_platforms=NULL;
            return CL_OUT_OF_HOST_MEMORY;
        }
        socket_flag |= Recv(sockfd,
                            server_platforms,
                            num_platforms * sizeof(pointer),
                            MSG_WAITALL);
        if(socket_flag){
            free(global_platforms); global_platforms=NULL;
            free(server_platforms); server_platforms=NULL;
            return CL_OUT_OF_HOST_MEMORY;
        }
        // Create the platform objects
        for(j = 0; j < num_platforms; j++){
            global_platforms[stored_platforms + j] = (cl_platform_id)malloc(
                    sizeof(struct _cl_platform_id));
            if(!global_platforms[j]){
                for(i = 0; i < stored_platforms + j; i++){
                    free(global_platforms[i]);
                    global_platforms[i] = NULL;
                }
                free(global_platforms); global_platforms=NULL;
                free(server_platforms); server_platforms=NULL;
                return CL_OUT_OF_HOST_MEMORY;
            }
            global_platforms[stored_platforms + j]->ptr_on_peer = server_platforms[j];
            global_platforms[stored_platforms + j]->dispatch = dispatch;
            global_platforms[stored_platforms + j]->server = servers[i];
            global_platforms[stored_platforms + j]->num_devices = 0;
            global_platforms[stored_platforms + j]->devices = NULL;
            global_platforms[stored_platforms + j]->num_contexts = 0;
            global_platforms[stored_platforms + j]->contexts = NULL;
        }

        free(server_platforms); server_platforms = NULL;
        stored_platforms += num_platforms;
    }

    // Discard the platforms which are not supporting OpenCL 1.2
    // It is not safe to operate with such platforms due to we cannot ask
    // for the kernel arguments address, and therefore we cannot
    // determine the right server object references
    i = 0;
    while(i < num_global_platforms){
        size_t version_size = 0;
        flag = getPlatformInfo(global_platforms[i],
                                   CL_PLATFORM_VERSION,
                                   0,
                                   NULL,
                                   &version_size);
        if(flag != CL_SUCCESS){
            VERBOSE("Discarded platform (clGetPlatformInfo failed)!\n");
            discardPlatform(global_platforms[i]);
            continue;
        }
        char *version = (char*)malloc(version_size);
        if(!version){
            VERBOSE("Discarded platform (CL_OUT_OF_HOST_MEMORY)!\n");
            discardPlatform(global_platforms[i]);
            continue;
        }
        flag = getPlatformInfo(global_platforms[i],
                                   CL_PLATFORM_VERSION,
                                   version_size,
                                   version,
                                   NULL);
        if(flag != CL_SUCCESS){
            VERBOSE("Discarded platform (clGetPlatformInfo failed)!\n");
            discardPlatform(global_platforms[i]);
            continue;
        }

        char *toread = NULL;
        size_t nchars = strlen("OpenCL ");
        unsigned int major_version=0, minor_version=0;
        if(strncmp(version, "OpenCL ", nchars * sizeof(char))){
            VERBOSE("Discarded platform (version string should start by OpenCL)!\n");
            discardPlatform(global_platforms[i]);
            continue;
        }
        toread = &(version[nchars]);
        nchars = strcspn (toread, ".");
        if(nchars == strlen(toread)){
            VERBOSE("Discarded platform (Bad OpenCL version string)!\n");
            discardPlatform(global_platforms[i]);
            continue;
        }
        major_version = strtol(toread, NULL, 10);
        toread = &(toread[nchars + 1]);
        minor_version = strtol(toread, NULL, 10);

        if((major_version <= 1) && (minor_version <= 1)){
            VERBOSE("Discarded platform (OpenCL %u.%u)!\n",
                    major_version, minor_version);
            discardPlatform(global_platforms[i]);
            continue;
        }
        free(version); version = NULL;
        i++;
    }

    return CL_SUCCESS;
}

cl_int getPlatformIDs(cl_uint                   num_entries,
                      struct _cl_icd_dispatch*  dispatch,
                      cl_platform_id*           platforms,
                      cl_uint*                  num_platforms)
{
    cl_int flag;
    // Obtain all the platforms from the servers
    flag = initPlatforms(dispatch);
    if(flag != CL_SUCCESS){
        return flag;
    }

    if(num_platforms){
        *num_platforms = num_global_platforms;
    }

    if((!num_global_platforms) || (!num_entries)){
        return CL_SUCCESS;
    }

    cl_uint n = (num_entries < num_global_platforms) ? num_entries : num_global_platforms;
    memcpy(platforms, global_platforms, n * sizeof(cl_platform_id));

    return CL_SUCCESS;
}

cl_int getPlatformInfo(cl_platform_id    platform,
                       cl_platform_info  param_name,
                       size_t            param_value_size,
                       void *            param_value,
                       size_t *          param_value_size_ret)
{
    cl_int flag = CL_OUT_OF_HOST_MEMORY;
    int socket_flag = 0;
    size64 size_ret;
    void *value_ret = NULL;
    unsigned int comm = ocland_clGetPlatformInfo;
    if(param_value_size_ret) *param_value_size_ret = 0;

    int *sockfd = platform->server->socket;
    if(!sockfd){
        return CL_INVALID_PLATFORM;
    }

    // Useful data to be append to specific cl_platform_info queries
    const char* ip = oclandServerAddress(*sockfd);
    const char* ocland_pre = "ocland(";
    const char* ocland_pos = ") ";
    size64 param_value_size_64 = param_value_size;

    socket_flag |= Send(sockfd, &comm, sizeof(unsigned int), MSG_MORE);
    socket_flag |= Send(sockfd, &(platform->ptr_on_peer), sizeof(pointer), MSG_MORE);
    socket_flag |= Send(sockfd, &param_name, sizeof(cl_platform_info), MSG_MORE);
    socket_flag |= Send(sockfd, &param_value_size_64, sizeof(size64), 0);
    socket_flag |= Recv(sockfd, &flag, sizeof(cl_int), MSG_WAITALL);
    if(socket_flag){
        return CL_OUT_OF_HOST_MEMORY;
    }
    else if(flag != CL_SUCCESS){
        return flag;
    }
    socket_flag |= Recv(sockfd, &size_ret, sizeof(size64), MSG_WAITALL);
    if(socket_flag){
        return CL_OUT_OF_HOST_MEMORY;
    }
    if(param_value_size){
        value_ret = malloc(size_ret);
        if(!value_ret){
            return CL_OUT_OF_HOST_MEMORY;
        }
        socket_flag |= Recv(sockfd, value_ret, size_ret, MSG_WAITALL);
        if(socket_flag){
            return CL_OUT_OF_HOST_MEMORY;
        }
    }
    // Attach the ocland identifier if needed
    if((param_name == CL_PLATFORM_NAME) ||
       (param_name == CL_PLATFORM_VENDOR)){
        // We need to add an identifier
        size_ret += strlen(ocland_pre) + strlen(ip) + strlen(ocland_pos);
        if(value_ret){
            char* backup = value_ret;
            value_ret = malloc(size_ret);
            sprintf(value_ret, "%s%s%s%s", ocland_pre,
                                           ip,
                                           ocland_pos,
                                           backup);
            free(backup); backup = NULL;
        }
    }
    // Copy the answer to the output vars
    if(param_value_size_ret) *param_value_size_ret = (size_t)size_ret;
    if(param_value){
        if(param_value_size < size_ret){
            free(value_ret); value_ret = NULL;
            return CL_INVALID_VALUE;
        }
        memcpy(param_value, value_ret, size_ret);
        free(value_ret); value_ret = NULL;
    }

    return CL_SUCCESS;
}
