import sys
import subprocess
import io
import os

intermediate_filename = "intermezzo.cnf"
d = {}
name_map = []
n_vals = 0

def runsat(filename):
	os.system("export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/jhjr_math")
	os.system("./3sat -i {0} > encoded.out".format(filename))

def decode():
	#os.system("")
	poss_solutions = []
	actual_solutions = []
	f = open("encoded.out", "r")
	fi = f.read().split('\n')
	fi = fi[2:-2]
	for line in fi:
		line = line.split(" ")
		poss_solutions.append(line[4])
	for sol in poss_solutions:
		binary = list(bin(int(sol, 16))[2:])
		binary.reverse()
		a = ['0' for i in range(int(n_vals) - len(binary))]
		actual_solutions.append([binary.append(x) for x in a])

	for order in actual_solutions:
		i = 0
		solution = []
		while (order):
			i = i % len(order)
			if (order[i] == 0):
				s.append(name_map[i])
				order.pop(i)
			if 
			i = i + 1
	f.close()

def writeout(solutions):
	f = open("solutions.txt", "w+")
	for s in solutions:
		f.write(s + "\n")
	f.close()

def main(args):
	assert len(args) == 2, "Incorrect number of args. Please specify an input file."
	init_names = []
	constraints = []
	out_constraints = []
	try:
		f = open(args[1], "r")
		n_vals = f.readline().strip()
		init_names = f.readline().strip().split(" ")
		n_const = 2 * int(f.readline().strip())
		
		i = 0
		line = "filler!"
		while (line != ''):
			line = f.readline().strip()
			if line == "":
				break
			line = [x for x in line.split(" ")]
			constraints.append(line)
			i = i + 1
		f.close()

	except IOError:
		print "File %s not found" % args[1]

	for i, name in enumerate(init_names):
		d[name] = i+1
		name_map.insert(i, name)
	
	for con in constraints:
		entry1 = "{0} {1} -{2} 0\n".format(d[con[0]], d[con[1]], d[con[2]])
		entry2 = "-{0} -{1} {2} 0\n".format(d[con[0]], d[con[1]], d[con[2]])
		out_constraints.append(entry1)
		out_constraints.append(entry2)
		
	try:
		out = open(intermediate_filename, "w+")
		out.write("p cnf {0} {1}\n".format(n_vals,n_const))
		for line in out_constraints:
			out.write(line)
		out.close()

	except IOError:
		print "Something went wrong with writing to the output."
	return n_vals
	
n_vals = main(sys.argv)
runsat(intermediate_filename)
decode()
