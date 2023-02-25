# Http Server

This is an http/1.0 and http/1.1 server written in c. It is able to take in requests from browsers and pass back images, mp4s, as well as regular html files. It uses multithreading to do this work.

There is an additional file that shows a server implemented using an event driven queue that passes messages from a receiver to a thread pool. The functionality is the same, but the efficiency is much higher, especially for concurrent requests.