# stream

A simple BISS (http://en.wikipedia.org/wiki/Basic_Interoperable_Scrambling_System) decryption applicatiın with VLC's libdvbcsa (http://www.videolan.org/developers/libdvbcsa.html) library. Application reads selected MPEG transport stream (http://en.wikipedia.org/wiki/MPEG_transport_stream) from a dump .ts file and sends over network as IP multicasting. 

Command line arguments are .ts file, service ID to multicast in stream file, multicasting IP address, multicasting port, BISS key

Note that normally MPEG is streamed in real time but in this application, stream is read from file so synchronization has to be done by manually with usleep() function in main loop. (Sorry very lame solution but it is unnecessary to sync stream in this situation)
