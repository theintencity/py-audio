from distutils.core import setup, Extension

module1 = Extension('audiodev', sources = ['audiodev.cpp'],
                    include_dirs = ['rtaudio'],
                    libraries = ['pthread', 'asound'], extra_link_args = ['rtaudio/librtaudio.a'])
module2 = Extension('audiospeex', sources = ['audiospeex.cpp'],
                    include_dirs = ['speex/include'],
                    library_dirs = ['speex/libspeex/.libs'],
                    libraries = ['speex', 'speexdsp'], extra_link_args = ['-fPIC'])

import os
# libdir = 'flite/build/x86_64-linux-gnu'


module3 = Extension('audiotts', sources = ['audiotts.cpp'],
                    include_dirs = ['flite/include'],
                    library_dirs = ['flite/build/x86_64-linux-gnu/lib'],
                    libraries = ['flite_cmu_us_kal', 'flite_cmu_us_awb', 'flite_cmu_us_rms', 'flite_cmu_us_slt',
                    			 'flite_usenglish', 'flite_cmulex', 'flite'],
                    extra_link_args = ['-fPIC'])

setup (name = 'PackageName', version = '1.0',
       description = 'audio device and codecs module',
       ext_modules = [module1, module2, module3])