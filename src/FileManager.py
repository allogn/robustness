import json
import os
import shutil
import logging

class FileManager:
    def __init__(self, title):
        self.title = title
        assert(title != None)
        self.data_dir = "robustFiles"
        if not os.path.isdir(self.get_root_path()):
            raise Exception("Data path does not exist")

    def get_data_path(self):
        return os.path.join(self.get_root_path(), self.data_dir, self.title)

    def get_relative_data_path(self):
        return os.path.join(self.data_dir, self.title)

    def clean_data_path(self):
        self.clean_path(self.get_data_path())

    @staticmethod
    def get_root_path(relative_path = ''):
        return os.path.join(os.environ['ALLDATA_PATH'], relative_path)

    def remove_all(self):
        self.remove_path(self.get_data_path())

    @staticmethod
    def list_subdirs(mypath):
        return [f for f in os.listdir(mypath) if not os.path.isfile(os.path.join(mypath, f))]

    @staticmethod
    def list_files_with_ext(path, ext, recursive=False):
        if not os.path.isdir(path):
            raise Exception("{} does not exist".format(path))
        l = [f for f in FileManager.list_files(path) if os.path.splitext(f)[1] == "." + ext]
        if recursive:
            for d in FileManager.list_dirs(path):
                l += FileManager.list_files_with_ext(d, ext, True)
        return l

    @staticmethod
    def list_files(mypath):
        return [os.path.join(mypath, f) for f in os.listdir(mypath) if os.path.isfile(os.path.join(mypath, f))]

    @staticmethod
    def list_dirs(mypath):
        return [os.path.join(mypath, d) for d in os.listdir(mypath) if os.path.isdir(os.path.join(mypath, d))]

    @staticmethod
    def clean_path(path):
        if os.path.isdir(path):
            FileManager.remove_path(path)
        FileManager.create_path(path)

    @staticmethod
    def remove_path(path):
        shutil.rmtree(path)

    @staticmethod
    def create_path(path):
        if not os.path.isdir(path):
            os.makedirs(path)

    @staticmethod
    def is_file(file_full_path):
        return os.path.isfile(file_full_path)
