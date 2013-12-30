"""
"""
from __pyconf import *


class ConfigSection(object):
    def __init__(self, dict_vals):
        for key, val in dict_vals.items():
            if isinstance(val, dict):
                self.__dict__[key] = ConfigSection(val)
            else:
                self.__dict__[key] = val

class Config(ConfigSection):
    def __init__(self, file_path):
        ConfigSection.__init__(self, parse_to_dict(file_path)) 
        self.__file_path = file_path 

