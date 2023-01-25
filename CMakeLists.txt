cmake_minimum_required(VERSION 3.10.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Socket)

############################
###  Configured Headers  ###
############################

#####################################
###  Global Compiler Definitions  ###
#####################################

##########################
###  Dependency Setup  ###
##########################

##########################
###  Global Variables  ###
##########################
set(SOCKET_INCLUDES_LIST	"${CMAKE_SOURCE_DIR}/include")

#################################
###  Compiler Specific Fixes  ###
#################################

########################
###  Subdirectories  ###
########################
add_subdirectory(test)
