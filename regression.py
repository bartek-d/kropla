#!/bin/env python3

from subprocess import PIPE, Popen
from threading  import Thread
from queue import Queue, Empty
import sys
import time
import re

def enqueue_output_o(out, queue):
    f = open("log_o.txt", "a+t")
    for line in out:
        f.write(line)
        queue.put(line)
    f.close()

def enqueue_output_e(out, queue):
    f = open("log_e.txt", "a+t")
    for line in out:
        f.write(line)
        queue.put(line)
    f.close()

def getValue(line, prefix):
    return line[len(prefix):-1]

def collectTests():
    tests = []
    moves = []
    name = None
    sgf = None
    time_msec = None
    default_time_msec = "10000"
    default_value = 0.0
    with open("../regression-list.txt") as regrf:
        for line in regrf:
            if line.startswith('name='):
                name = getValue(line, "name=")
                default_value = 0.0
            elif line.startswith('sgf='):
                sgf = getValue(line, "sgf=")
            elif line.startswith('time='):
                time_msec = getValue(line, "time=")
            elif line.startswith('move='):
                move, pts = getValue(line, "move=").split()
                moves.append((move, float(pts)))
            elif line.startswith('else='):
                default_value = float(getValue(line, "else="))
            elif line.startswith('end'):
                if name == None or sgf == None or len(moves) == 0:
                    print(f"Ignoring a noncomplete test: name = {name}, sgf = {sgf}, moves = {moves}", file=sys.stderr)
                else:
                    if time_msec == None:
                        time_msec = default_time_msec
                    tests.append({'name': name, 'sgf': sgf, 'moves': moves[:], 'time': time_msec, 'default_value': default_value})
                moves = []
                name = None
                sgf = None
                time_msec = None
    return tests

def getScoreForMove(ai_move, test):
    for (move, score) in test['moves']:
        if ai_move == move:
            return score
    return test['default_value']

def runSingleTestOnce(test, *args):
    args = ['./kropla'] + list([*args])
    process = Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE, bufsize=1, encoding='utf-8')
    qerr = Queue()
    qout = Queue()
    tout = Thread(target=enqueue_output_o, args=(process.stdout, qout))
    tout.start()
    terr = Thread(target=enqueue_output_e, args=(process.stderr, qerr))
    terr.start()
    process.stdin.write(test['sgf'] + test['time'] + '\n')
    process.stdin.flush()
    answer = None
    timeout = 0.1
    block = True
    loop_count = 0
    playouts = -1
    cnnReads = -1
    mikros = -1
    while True:
        time.sleep(0.65)
        while True:
            try:
                line = qerr.get(block, timeout)
                if line.startswith("Real saved playouts:"):
                    g = re.search("Real saved playouts: (\d*);.*cnnReads: (\d*).*", line)
                    playouts = int(g.group(1))
                    cnnReads = int(g.group(2))
                if line.startswith("Total time:"):
                    g = re.search("Total time: (\d*) mikros", line)
                    mikros = int(g.group(1))
                time.sleep(0.001)
            except Empty:
                break
        while True:
            try:
                answer = qout.get(block, timeout)
                time.sleep(0.001)
            except Empty:
                break
            else:
                if answer.startswith('(;SZ['):
                    break
        if answer is not None:
            break
        loop_count += 1
        if loop_count > 90:
            raise Exception("timeout")
    process.stdin.write('END\n')
    process.stdin.flush()
    moves = answer.split(';')
    lastBracket = moves[-1].index(']')
    ai_move = moves[-1][2:lastBracket]
    score = getScoreForMove(ai_move, test)

    join_timeout = 5
    tout.join(join_timeout)
    terr.join(join_timeout)
    return (ai_move, score, playouts, cnnReads, mikros)

def runSingleTestManyTimes(test, how_many_times, usecnnonly):
    print(f"Running test {test['name']}...", file=sys.stderr)
    msec = test['time']
    results = {}
    total_score = 0.0
    total_playo = 0
    total_mikros = 0
    total_cnnReads = 0
    iterations = '-1' if usecnnonly else '5000000'
    for i in range(how_many_times):
        while True:
            try:
                ai_move, score, playouts, cnnReads, mikros  = runSingleTestOnce(test, '-', '10000', iterations, str(test['threads']), str(msec))
                break
            except:
                print(f"  {i}: timeout, retry", file=sys.stderr)
        print(f"  {i}: {ai_move} --> {score}; playouts={playouts}, cnnReads={cnnReads}, playouts/s={playouts*1e6/mikros:.1f}", file=sys.stderr)
        total_mikros += mikros
        total_playo += playouts
        total_cnnReads += cnnReads
        if ai_move not in results:
            results[ai_move] = {'score' : score, 'times' : 1}
        else:
            results[ai_move]['times'] += 1
        total_score += score
    print(f"  total_score: {total_score}, mean: {total_score / how_many_times}", file=sys.stderr)
    results['total'] = total_score
    results['playoutsps'] = total_playo * 1e6 / total_mikros
    results['cnnreads'] = total_cnnReads / how_many_times
    return results

tests = collectTests()
print("Tests found: ", len(tests), file=sys.stderr)
how_many_times = 10

check_name = lambda s : True

if len(sys.argv) > 1:
    check_name = lambda s : re.fullmatch(sys.argv[1], s)
    print(f"Regular expression: {sys.argv[1]}")

usecnnonly = len(sys.argv) > 2

try:
    lines = []
    with open("cnn.config") as conf:
        for line in conf:
            lines.append(line)
    if len(lines) >= 3:
        cnn_name = f"{lines[0].strip()} planes; name of file: {lines[2].split('/')[-1].strip()}"
except:
    cnn_name = "(unknown)"
print(f"CNN: {cnn_name}")

total = 0.0
number_of_tests = 0
for test in tests:
    test['threads'] = 80
    if not check_name(test['name']):
        print(f"Omitting {test['name']}")
        continue
    result = runSingleTestManyTimes(test, how_many_times, usecnnonly)
    number_of_tests += 1
    print(f"name={test['name']}")
    print(f"time ms={test['time']}")
    print(f"threads={test['threads']}")
    print(f"result={result}")
    print(f"playouts/s={result['playoutsps']:.2f}\n")
    print(f"mean={result['total'] / how_many_times}\n")
    total += result['total']

print(f"Total tests: {number_of_tests}, total score: {total}, mean score: {total / (how_many_times * number_of_tests)}.")

