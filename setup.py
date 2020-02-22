import sys
from setuptools import setup
if sys.version_info < (3,7):
    sys.exit('Sorry, Python < 3.7 is not supported') # required to have sorted dict

setup(name='src',
      version='0.0.1'
)
