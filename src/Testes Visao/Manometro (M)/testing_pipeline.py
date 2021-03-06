#!/usr/bin/env python
# -*- coding: utf-8 -*-
import numpy as np
import cv2
from matplotlib import pyplot as plt
import operator
from math import atan2
def radius(x):
   return x[2]

files = ['/home/gonzales/manometro.jpg']

radianos_manometro_0 = -0.737011559754
valor_manometro_90 = 5

print("[INFO] starting video stream...")
vs = cv2.VideoCapture(1)

# loop over the frames from the video stream
while True:
    # grab the frame from the threaded video stream and resize it to
    # have a maximum width of 400 pixels
    ret, img = vs.read()
    plt.figure()
    plt.imshow(img)

    img = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
    img = cv2.inRange(img, (0, 0, 100), (180, 255, 255))

    # Detecta o Circulo do manometro
    circles = cv2.HoughCircles(img,cv2.HOUGH_GRADIENT,1,minDist=1,
                                param1=30,param2=50,minRadius= 10,maxRadius=0)

    if circles is not None:
        print("Nenhum circulo detectado")

        # Corta um circulo interior à esse
        height,width = img.shape
        mask = np.zeros((height,width), np.uint8)

        circles = np.uint16(np.around(circles))
        circle = max(circles[0], key=radius)

        cv2.circle(mask,(circle[0],circle[1]),int(circle[2]),(255,255,255),thickness=-1) # diminui o raio do circulo
        img = cv2.bitwise_and(img, img, mask=mask)

    # Usa transformação morfológica CLOSE para manter apenas o ponteiro
    kernel = np.ones((4,4),np.uint8)
    img = cv2.morphologyEx(img, cv2.MORPH_CLOSE, kernel)

    # Encontra Contornos
    img, contours, hierarchy = cv2.findContours(img,cv2.RETR_TREE,cv2.CHAIN_APPROX_SIMPLE)

    areas=[]
    for i in range(len(contours)):
        areas.append(cv2.contourArea(contours[i]))

    # Encontra o contorno de maior área e exclui (era o do manômetro externo)
    max_index  = areas.index(max(areas))
    del contours[max_index] # this is the outside circle
    cnt  = max(contours, key = cv2.contourArea)

    # Desenha o menor polígono convexo possível
    hull = cv2.convexHull(cnt,returnPoints = True)
    cv2.drawContours(img,[hull],0,(0,0,255),2)

    M = cv2.moments(hull)
    cx = int(M['m10']/M['m00'])
    cy = int(M['m01']/M['m00'])
    cx_hull = int(M['m10']/M['m00'])
    cy_hull = int(M['m01']/M['m00'])

    cv2.circle(img,(cx,cy),10,(255,255,255),thickness=-1) # diminui o raio do circulo

    M = cv2.moments(cnt)
    cx = int(M['m10']/M['m00'])
    cy = int(M['m01']/M['m00'])
    cx_cnt = int(M['m10']/M['m00'])
    cy_cnt = int(M['m01']/M['m00'])

    cv2.circle(img,(cx,cy),10,(127,127,127),thickness=-1) # diminui o raio do circulo

    # Ajusta uma reta
    line = cv2.fitLine(points=hull, distType=cv2.DIST_L2, param=0, reps=0.01, aeps=0.01)

    posicao_relativa = atan2(cy_cnt-cy_hull,cx_cnt-cx_hull)
    print(posicao_relativa*180/np.pi)

    rows, cols = img.shape
    vx = line[0]
    vy = line[1]
    x = line[2]
    y = line[3]
    lefty = round( (-x * vy / vx) + y)
    righty = round((( cols - x) * vy / vx) + y)
    point1 = ( int( cols - 1), int(righty))
    point2 = ( int( 0), int(lefty))
    angulo = atan2(vy,vx)
    cv2.line(img, point1, point2, (127,127,127), 2, cv2.LINE_AA, 0)

    print(atan2(vy,vx))
    
    resultado = 0
    if posicao_relativa > 0:
        if angulo > 0:
            resultado = angulo
        if angulo < 0:
            resultado = angulo + np.pi
    if posicao_relativa < 0:
        if angulo < 0:
            resultado = angulo
        if angulo > 0:
            resultado = angulo - np.pi

    resultado = (valor_manometro_90-0) / (np.pi/2 - radianos_manometro_0) * (resultado - radianos_manometro_0)
    print(resultado)
    plt.figure()
    plt.imshow(img, 'gray')

plt.show()


