import os
from distutils.core import setup, Extension

module = Extension('inet_util', sources=['inet_util.c'], libraries=[
                   'ws2_32'] if os.name == 'nt' else [])

setup(name='inet_util',
      version='1.0.0',
      description='inet_pton and inet_ntop for socket',
      ext_modules=[module])
