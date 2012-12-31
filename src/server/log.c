/*
 *  This file is part of ocland, a free CFD program based on SPH.
 *  Copyright (C) 2012  Jose Luis Cercos Pita <jl.cercos@upm.es>
 *
 *  ocland is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ocland is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ocland.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <ocland/server/log.h>

/// Log file path
static char* log_path = NULL;

int setLogFile(const char* path)
{
    // Clean
    if(log_path) free(log_path); log_path=NULL;
    // Get log file path
    log_path = (char*)malloc((strlen(path)+1)*sizeof(char));
    strcpy(log_path, path);
    // Test if file can be touched
    FILE *test = fopen(log_path, "w");
    if(!test){
        if(log_path) free(log_path); log_path=NULL;
        return 0;
    }
    fclose(test);
    // Redirects standart outputs
    freopen(log_path, "a+", stdout);
    freopen(log_path, "a+", stderr);
    return 1;
}

int initLog()
{
    if(log_path)
        return 1;
    if(!setLogFile("/var/log/ocland.log")){
        printf("File \"/var/log/ocland.log\" could not be opened!\n");
        return 0;
    }
    return 1;
}
