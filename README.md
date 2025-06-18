#lib-nixtla-audio

Nixtla ("time lapse" in Nahuatl) is a C library for audio playback and capture that abstracts the implementation of OpenAL and OpenSL.

This abstraction allows you to create code that can be compiled and run on iOS, Android, BB10, Windows, Linux, MacOSX, and other operating systems.

#Licence

LGPLv2.1, see LICENSE file

#Origin

Published on 15/Mar/2014.

This library was initially developed by Marcos Ortega based on experiences in the development of multimedia applications.

#Features

- Memory management using a RETAIN_COUNT model (RETAIN/RELEASE).
- OpenAL-based sources and buffers model.
- OpenSL-based callback model.
- Only two files (nixtla-audio.h and nixtla-audio.c)

Additionally:

- Memory management is customizable using the NIX_MALLOC and NIX_FREE macros.
- The point of invocation of callbacks is controlled by the user.

#Utility

Mainly in native multiplatform projects, including video games.

#How to integrate in your project?

Nixtla's design prioritizes ease of integration with other projects through simple steps such as:

- Point to or copy the nixtla-audio.h and nixtla-audio.c files to the project.
- Link the project with OpenSL (Android) or OpenAL (iOS, BB10, windows, MacOSx, Linux and others)

#Examples and Demos

This repository includes demo projects for the following environments:

- XCode
- Eclipse (android)
- Eclipse (linux)
- Visual Sutido
