import sys, os
sys.path.append(os.path.join(os.path.abspath(''), '..', 'src'))
from Experiment import *
import json
import datetime

# set up logging to file - see previous section for more details
logging.basicConfig(level=logging.INFO,
                    format='%(asctime)-3s :: %(levelname)-3s :: %(threadName)s :: %(message)s',
                    datefmt='%m-%d %H:%M',
                    filename=os.path.join(FileManager.get_root_path(),'parallelWrapper.log'),
                    filemode='a')
# define a Handler which writes INFO messages or higher to the sys.stderr
console = logging.StreamHandler()
console.setLevel(logging.WARNING)
# set a format which is simpler for console use
formatter = logging.Formatter('%(levelname)-8s %(relativeCreated)6d %(threadName)s: %(message)s')
# tell the handler to use this format
console.setFormatter(formatter)
# add the handler to the root logger
logging.getLogger('').addHandler(console)

if __name__ == "__main__":
	scriptname = sys.argv[1]
	with open(os.path.join(os.path.abspath(''),'..','dags',scriptname + '.json'),'r') as f:
		params = json.load(f)
	params['tag'] = scriptname
	print("Experiment {} started at {}".format(scriptname, datetime.datetime.now()))
	experiment = Experiment(params)
	experiment.run()
