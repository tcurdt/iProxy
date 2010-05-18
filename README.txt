At least when you don't want to jailbreak your iPhone so far you've had only one option if you want to connect from your laptop via iPhone to the internet. And that's paying an additional fee to your mobile carrier to provide you with the signed tethering profile. If you are not with one of the "official" providers you probably didn't even have that option at all. (Unless you are running a pre 3.1 OS version - which again is not acceptable for many of us.)

iProxy does not give you tethering - it just gives you the next best thing. A socks proxy on your iPhone. Essentially what the famous netshare app did before it got pulled from the App Store.

iProxy is not as convenient as the real tethering. The internet connection is a few clicks more away. But if you've got a developer certificate (or have a friend that has one) it certainly is cheaper than handing out the money to your favorite telco. Especially if you only need this connection every now and then.

iProxy is just an evening hack of a project so it's far from perfect. But iProxy is released under the Apache license and freely available on github. So fork away if you want to see something fixed.

This is to give back to the community ...and to stop the ripoff madness.

Feel free to give some feedback via twitter.

Torsten
@tcurdt


Addition for iPad "fake" tethering

Added a small http server that served a .pac file automatically. This simplifies 
fake-tethering for the iPad.

Create a network and give the iPhone the IP nr 10.0.0.1 (Google will help you find out how)

Enter ''http://10.0.0.1:8080/socks.pac'' on the iPad in the URL field for the
networks HTTP Proxy Auto configuration.

I have written a little about this here http://www.memention.com/blog/2010/05/15/Removing-a-step.html

Edward
@e_patel
