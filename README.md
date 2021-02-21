# deskctrl
Arduino code to control a Jarvis JRV1 standing desk.
Simple Python script to talk to the Arduino over serial and expose a HTTP interface, which you can integrate with Amazon Echo.

I am uploading this as a portfolio of things I've done, instead of expecting anyone to be able to use this.

Yes, the code is in "Blink.ino" because I modified it from some "blink the lights" example.

Yes, the HTTP interface uses security through obscurity. The path ("/sarahsdesk") to PUT a new command to is essentially the password. But don't try to hack me -- I'm aware that the "password" is now public on GitHub.

Here it is in action:
https://www.youtube.com/watch?v=m35kcOKfvk0
