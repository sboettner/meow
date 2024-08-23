# Meow
Meow started as a project because there does not seem to exist a good free and open solution to correct the pitch of vocal recordings
(nor any commercial software supported on the Linux platform).

Since pitch shifting and time stretching are roughly equivalent problems from a computational point of view,
it was an obvious decision to also implement timing correction right from the start.

Currently, Meow is an early prototype and proof-of-concept, but fully functional. I am already about to use it for production,
but expect to see occasional bugs and crashes. Below is a glimpse of what the user interface looks like:

![Intonation Editor Screenshot](/assets/intonationeditor.png)


# Platform Support
Meow should be fairly portable but has only been built and tested on Linux so far. 
Its user interface has been implemented using Gtk (version 3) which technically supports multiple platforms,
but may feel distinctively non-native on other platforms than Linux. However, the architecture of Meow has
been designed in a way that allows the user interface to be easily replaced, possibly using a different framework.
