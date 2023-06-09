# Object Oriented Socket
Written by James Horner

## About
The Socket class provides an easy to use C++ interface to C sockets where most of the setup is handled by the class. The class provides methods to pre-configure a destination for packets, send packets based on parameters, receive packets, as well as granting access to the socket file descriptor in case lower level configuration is required. Error handling is done through custom errors which contain textual information about errors that have occurred, so that it is up to the user to handle them. The class is header-only so it should be pretty self explanatory how to include it in a project, however one thing to note is that platform specific system libraries will need to be linked to for network programming.

## Contact Info
James Horner
James.Horner@nrc-cnrc.gc.ca or jwehorner@gmail.com