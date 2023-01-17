# Object Oriented Socket
Written by James Horner (adapted from code by Kris Ellis, Bill Gubbels, et. al.)

## About
The Socket class provides an easy to use C++ interface to C sockets where most of the setup is handled by the class. The class provides methods to pre-configure a destination for packets, send packets based on parameters, receive packets, as well as granting access to the socket file descriptor in case lower level configuration is required. Error handling is done through std::runtime_errors which contain textual information about errors that have occurred, so that it is up to the user to handle them. The class is header-only so it should be pretty self explanatory how to include it in a project.

## Contact Info
James Horner
James.Horner@nrc-cnrc.gc.ca or JamesHorner@cmail.carleton.ca