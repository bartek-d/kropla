#!/bin/env python3

from subprocess import PIPE, Popen
from threading  import Thread
from queue import Queue, Empty
import sys
import time

def enqueue_output(out, queue):
    for line in out:
        queue.put(line)

def getValue(line, prefix):
    return line[len(prefix):-1]

def collectTests():
    tests = []
    moves = []
    name = None
    sgf = None
    time_msec = None
    default_time_msec = "10000"
    with open("regression-list.txt") as regrf:
        for line in regrf:
            if line.startswith('name='):
                name = getValue(line, "name=")
            elif line.startswith('sgf='):
                sgf = getValue(line, "sgf=")
            elif line.startswith('time='):
                time_msec = getValue(line, "time=")
            elif line.startswith('move='):
                move, pts = getValue(line, "move=").split()
                moves.append((move, float(pts)))
            elif line.startswith('end'):
                if name == None or sgf == None or len(moves) == 0:
                    print(f"Ignoring a noncomplete test: name = {name}, sgf = {sgf}, moves = {moves}", file=sys.stderr)
                else:
                    if time_msec == None:
                        time_msec = default_time_msec
                    tests.append({'name': name, 'sgf': sgf, 'moves': moves[:], 'time': time_msec})
                moves = []
                name = None
                sgf = None
                time_msec = None
    return tests

def getScoreForMove(ai_move, test):
    for (move, score) in test['moves']:
        if ai_move == move:
            return score
    return 0.0

def runSingleTestOnce(test, *args):
    args = ['bin/kropla'] + list([*args])
    process = Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE, bufsize=1, encoding='utf-8')
    qerr = Queue()
    qout = Queue()
    tout = Thread(target=enqueue_output, args=(process.stdout, qout))
    tout.start()
    terr = Thread(target=enqueue_output, args=(process.stderr, qerr))
    terr.start()
    process.stdin.write(test['sgf'] + test['time'] + '\n')
    process.stdin.flush()
    answer = None
    while True:
        time.sleep(0.65)
        while True:
            try:
                line = qerr.get_nowait()
            except Empty:
                break
        while True:
            try:
                answer = qout.get_nowait()
            except Empty:
                break
            else:
                if answer.startswith('(;SZ['):
                    break
        if answer is not None:
            break
    process.stdin.write('END\n')
    process.stdin.flush()
    moves = answer.split(';')
    lastBracket = moves[-1].index(']')
    ai_move = moves[-1][2:lastBracket]
    score = getScoreForMove(ai_move, test)
    return (ai_move, score)

def runSingleTestManyTimes(test, how_many_times):
    print(f"Running test {test['name']}...", file=sys.stderr)
    threads = 2
    msec = test['time']
    results = {}
    total_score = 0.0
    for i in range(how_many_times):
        ai_move, score = runSingleTestOnce(test, '-', '10000', '5000000', str(threads), str(msec))
        print(f"  {i}: {ai_move} --> {score}", file=sys.stderr)
        if ai_move not in results:
            results[ai_move] = {'score' : score, 'times' : 1}
        else:
            results[ai_move]['times'] += 1
        total_score += score
    print(f"  total_score: {total_score}, mean: {total_score / how_many_times}", file=sys.stderr)
    results['total'] = total_score
    return results

tests = collectTests()
print("Tests found: ", len(tests), file=sys.stderr)
how_many_times = 10

total = 0.0
for test in tests:
    result = runSingleTestManyTimes(test, how_many_times)
    print(f"name={test['name']}")
    print(f"result={result}")
    print(f"mean={result['total'] / how_many_times}\n")
    total += result['total']

print(f"Total tests: {len(tests)}, total score: {total}, mean score: {total / (how_many_times * len(tests))}.")

