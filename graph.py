import matplotlib.pyplot as plt
import random

file = open('graph.txt', 'r')
points =[]
output = {}

while True:
    line = file.readline()
    if not line:
        break
    if line[0:5] == "GRAPH":
        # print(line)
        spacesep = line.split()
        # print(line)
        # print(spacesep[2], spacesep[3], spacesep[4])
        pid = int(spacesep[2])
        q = int(spacesep[3])
        ticks = int(spacesep[4])
        # plt.scatter(ticks, q, pid)
        points.append((ticks, q, pid))

for x, y, pid in points:
    print("qw", x, y, pid)
    if pid in output:
        output[pid].append((x, y, pid))
    else :
        output[pid] = [(x, y, pid)]

# print(output)
for i in output:
    r = random.random()
    g = random.random()
    b = random.random()
    col = (r, g, b)
    # print(output[i])
    xval = [point[0] for point in output[i]]
    yval = [point[1] for point in output[i]]
    
    for point in output[i]:
        # print(point[0], point[1])
        plt.plot(xval, yval, c = col)

plt.xlabel("ticks")
plt.ylabel("q")
plt.show()