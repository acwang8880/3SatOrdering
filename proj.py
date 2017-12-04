import sys
import subprocess
import io
import os

intermediate_filename = "intermezzo.cnf"
d = {}
d_inverse = {}
n_vals = 0

def runsat(filename):
    os.system("export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/jhjr_math")
    os.system("./3sat -i {0} > encoded.out".format(filename))

def decode():
    #os.system("")
    poss_solutions = []
    actual_solutions = []
    f = open("encoded.out", "r")
    out = open("solutions.out", "w+")
    fi = f.read().split('\n')
    # no solution checker
    if fi[1][:1] == 'c':
        out.write("No Solution is possible with given constraints. Output >> solutions.out")
        return -1
    
    # solution printer
    fi = fi[2]
    print fi
    for line in fi:
        line = line.split(" ")
        poss_solutions.append(line[::-2])
        
        break

    

    for sol in poss_solutions:
        binary = list(bin(int(sol, 16))[2:])
        binary.reverse()
        a = ['0' for i in range(int(n_vals) - len(binary) + 1)]
        actual_solutions.append([binary.append(x) for x in a])

    for order in actual_solutions:
        i = 0
        s1 = []
        for i, e in enumerate(order):
            if e == 0:
                #s0.append(e)
                out.write("{0} ".format(d_inverse[i+1]))
            else:
                s1.append(i)
        for s in s1:
            out.write("{0} ".format(d_inverse[s+1]))
    f.close()
    out.close()
    return 0

#def writeout(solutions):
#    try:
#        
#        f = open("solutions.txt", "w+")
#        for s in solutions:
#            f.write(s + "\n")
#        f.close()
#    except IOError:
#        print "IO Error when writing to solutions.txt"

def main(args):
    assert len(args) == 2, "Incorrect number of args. Please specify an input file."
    constraints = []
    out_constraints = []
    try:
        f = open(args[1], "r")
        n_vals = int(f.readline().strip())
        #init_names = f.readline().strip().split(" ")
        n_const = 2 * int(f.readline().strip())
        
        i = 0

        # gets all the constraints as lines from the input file
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
    # TODO: Fix me. Add names to dictionary when you encounter them and add a new constraint clause for each new name found.
    #for i, name in enumerate(init_names):
    #    d_inverse[i+1] = name
    #    d[name] = i+1
    
    # Interprets every constraint into 3SAT cnf form. This must handle the checking and adding of new names.
    i = 1
    for con in constraints:
        for name in con:

            # if name is not already recorded, add it to the dictionaries. And add a new implicit constraint.
            if not name in d:
                d[name] = i
                d_inverse[i] = name
                #n_vals = 1 + n_vals
                i = i + 1
                if i > 3:
                    entry1 = "{0} {1} {2} 0\n".format(d[name] - 2, d[name] - 1, d[name])
                    entry2 = "-{0} -{1} -{2} 0\n".format(d[name] - 2, d[name] - 1, d[name])
                    out_constraints.append(entry1)
                    out_constraints.append(entry2)
                    n_const = n_const + 2
                    

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
if decode() < 0:
    print "No Solution Found"
else:
    print "Solution stored in solutions.txt"
