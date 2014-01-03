"""
"""
from json       import dumps
from __pyconf   import *


class ConfigSection(object):
    def __init__(self, dict_vals):
        for key, val in dict_vals.items():
            if isinstance(val, dict):
                self.__dict__[key] = ConfigSection(val)
            else:
                self.__dict__[key] = val 

    def as_dict(self):
        d = {}

        for key, val in self.__dict__.items():
            if isinstance(val, ConfigSection):
                d[key] = val.as_dict()
            else:
                d[key] = val

        return d

    def __str__(self):
        return dumps(self.as_dict(), indent = 4)

class Config(ConfigSection):
    def __init__(self, file_path):
        ConfigSection.__init__(self, parse_to_dict(file_path)) 

