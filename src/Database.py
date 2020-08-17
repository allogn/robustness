import datetime

class Database:
    '''
    Experiment tag is not unique for a row.
    Row id is unique and is returned by the save() function.

    Tag is set as global
    '''

    def __init__(self, tag = "tmp"):
        self.tag = tag

    def set_tag(self, tag):
        self.tag = tag

    def save(self, row):
        raise NotImplementedError()

    def load(self, row_id):
        raise NotImplementedError()

    def load_df_with_tag(self):
        raise NotImplementedError()

    def remove_tag(self, tag):
        raise NotImplementedError()

    def clean(self):
        self.remove_tag(self.tag)

    def enrich_row(self, row):
        row2 = dict(row)
        row2['tag'] = self.tag
        row2['created'] = str(datetime.datetime.now())
        return row2
