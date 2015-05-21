# Python bindings for audio device and speex codecs #

> This project was migrated from <https://code.google.com/p/py-audio> on May 17, 2015  
> Keywords: *Voice*, *Codec*, *Speex*, *Python*, *RtAudio*, *Resample*, *Device*  
> Members: *voipresearcher*, *theintencity*  
> Links: [Support](http://groups.google.com/group/myprojectguide), [Download](/py-audio1.0-python2.6-x86_64-MacOSX10.6.4.tgz) binary modules for Python 2.6 x86_64 on Mac OS X 10.6.4, Oct 2011, 114 KB, download count 315  
> License: [GNU Lesser GPL](http://www.gnu.org/licenses/lgpl.html)  
> Others: starred by 8 users  

This project builds the Python bindings for audio device interface, speex codecs library and text-to-speech library for real-time voice communication. In particular it provides Python wrapper modules for the following projects:

  1. [The RtAudio Project](http://www.music.mcgill.ca/~gary/rtaudio/) 4.0.8
  1. [The Speex Project](http://www.speex.org/) 1.2rc1
  1. [The Flite Project](http://www.speech.cs.cmu.edu/flite/) 1.4

## How to build? ##

Instructions to build on Unix-based platforms including Mac OS X and Linux using Python 2.6.

1. Checkout the latest version of this py-audio project from git.
```
  $ git clone https://github.com/theintencity/py-audio.git
  $ cd py-audio
```

2. Download [source](http://www.music.mcgill.ca/~gary/rtaudio/release/rtaudio-4.0.8.tar.gz), uncompress and build rtaudio
```
  $ tar -zxvf rtaudio-4.0.8.tar.gz
  $ cd rtaudio-4.0.8
  $ ./configure
  $ make
```
This will create the static and dynamic libraries in `rtaudio/`. We are interested only in static library `librtaudio.a`.

On Linux, if it complains about not found file, please change SHAREDLIB to SHARED at two places in its Makefile, and do `make` again.

3. Download [source](http://downloads.xiph.org/releases/speex/speex-1.2rc1.tar.gz), uncompress and build speex.
```
  $ tar -zxvf speex-1.2rc1.tar.gz
  $ cd speex-1.2rc1
  $ ./configure
  $ make
```
This will create the static and dynamic libraries in `libspeex/.libs/`. We are interested only in static libraries `libspeex.a` and `libspeexdsp.a`.

On Linux, if it complains about missing PIC option, change your configure command to `./configure CFLAGS=-fPIC` above and do configure and make again.

4. Download [source](http://www.speech.cs.cmu.edu/flite/packed/flite-1.4/flite-1.4-release.tar.bz2), uncompress and build flite.
```
  $ bunzip2 flite-1.4-release.tar.bz2
  $ tar -xvf flite-1.4-release.tar
  $ cd flite-1.4-release
  $ ./configure
  $ make
```
This will create the static libraries in `build/i386-darwin9.6.1/` or similar.

4. Create a new `setup.py` file in py-audio directory. On Mac OS X use the following content
```
from distutils.core import setup, Extension
module1 = Extension('audiodev', sources = ['audiodev.cpp'],
                    include_dirs = ['rtaudio-4.0.8'],
                    extra_link_args = ['rtaudio-4.0.8/librtaudio.a', '-framework', 'CoreAudio'])
module2 = Extension('audiospeex', sources = ['audiospeex.cpp'],
                    include_dirs = ['speex-1.2rc1/include'],
                    extra_link_args = ['speex-1.2rc1/libspeex/.libs/libspeex.a', 
                                                 'speex-1.2rc1/libspeex/.libs/libspeexdsp.a'])
import os
libdir = 'flite-1.4-release/build/%s-%s%s/lib'%(os.uname()[-1], 
             os.uname()[0].lower(), os.uname()[2])
module3 = Extension('audiotts', sources = ['audiotts.cpp'],
                    include_dirs = ['flite-1.4-release/include'],
                    extra_link_args = ['%s/lib%s.a'%(libdir, x) for x in (
                          'flite_cmu_us_kal', 'flite_cmu_us_awb', 'flite_cmu_us_rms', 
                          'flite_cmu_us_slt', 'flite_usenglish', 'flite_cmulex', 'flite')])
setup (name = 'PackageName', version = '1.1',
       description = 'audio device and codecs module',
       ext_modules = [module1, module2, module3])
```
On Linux, use the following content
```
from distutils.core import setup, Extension
module1 = Extension('audiodev', sources = ['audiodev.cpp'],
                    include_dirs = ['rtaudio-4.0.8'],
                    libraries = ['pthread', 'asound'], extra_link_args = ['rtaudio-4.0.8/librtaudio.a'])
module2 = Extension('audiospeex', sources = ['audiospeex.cpp'],
                    include_dirs = ['speex-1.2rc1/include'],
                    library_dirs = ['speex-1.2rc1/libspeex/.libs'],
                    libraries = ['speex', 'speexdsp'], extra_link_args = ['-fPIC'])
import os
libdir = 'flite-1.4-release/build/%s-%s%s'%(os.uname()[-1], 
             os.uname()[0].lower(), os.uname()[2])
module3 = Extension('audiotts', sources = ['audiotts.cpp'],
                    include_dirs = ['flite-1.4-release/include'],
                    library_dirs = [libdir],
                    libraries = ['flite_cmu_us_kal', 'flite_cmu_us_awb', 'flite_cmu_us_rms', 
                          'flite_cmu_us_slt', 'flite_usenglish', 'flite_cmulex', 'flite'],
                    extra_link_args = ['-fPIC'])
setup (name = 'PackageName', version = '1.0',
       description = 'audio device and codecs module',
       ext_modules = [module1, module2, module3])
```
On Linux, If a different sound device was used, then change the library from asound to that.

5. Compile the Python bindings. On Mac OS X use the following command after replacing the arch flag to i386 or ppc if applicable instead of x86\_64.
```
  $ ARCHFLAGS="-arch x86_64" python setup.py -v build
```
On Linux use the following command.
```
  $ python setup.py -v build
```
If you have multiple versions of Python installed, you may want to replace `python` to `python2.5` or `python2.6` as applicable in the above command. Note that python2.5 does not support x86\_64 on Mac OS X.

6. Copy the generated modules to the current directory.
```
  $ cp build/lib.*/audio*.so .
```
This will copy `audiospeex.so`, `audiodev.so` and `audiotts.so` to current `py-audio` directory.

## How to test? ##

An example file named test.py is available to allow you to test in loopback mode. Once you start the application, it opens the audio device, and for every captured frame it performs resampling and encoding/decoding, and finally plays back to the audio device.

```
$ python test.py
<ctrl-C to terminate>
```

An example file named tts.py is available to allow you to test text-to-speech feature. You can start it by supplying the text on command line.
```
$ python tts.py hello, how are you?
```

## How to use this in [SIP-RTMP](https://github.com/theintencity/rtmplite) gateway? ##

You only need to build `audiospeex.so` for transcoding. Place this module in your PYTHONPATH or in `rtmplite` directory so that it is accessible to `siprtmp.py` application. Then start `siprtmp.py` as shown on that project page with `-d` option, and it should detect the `audiospeex` module to enable transcoding.

## How to use this in another Python application? ##

The API is straightforward. You can use the built-in help command in Python to know the details. Moreover the example test.py illustrates how it works.

```
>>> import audiodev, audiospeex, audiotts
>>> help(audiodev)
>>> help(audiospeex)
>>> help(audiotts)
```
