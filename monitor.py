"""
monitor.py

Temperature/Humidity monitor using an orangePi, DS18B20 and DHT11. 
Data is displayed at thingspeak.com and updated to a caputred webcam images

Author: Michael A. Schulze
Website: mschulze.net

"""
#!/usr/bin/env python

import os
import sys
import subprocess
from time import sleep  
import urllib2
import re
import itertools
import ftplib 
import traceback

    
def getSensorData():
    # RH, T = Adafruit_DHT.read_retry(Adafruit_DHT.DHT11, 23)
	print 'top'
	# rel_hum, amb_temp, probe_temp = subprocess.check_output('./thermo')
	output = subprocess.check_output('./thermo')
    # return dict
	print output
	# print 'RH= ', rel_hum
	# print 'AT= ', amb_temp
	# print 'PT= ', probe_temp
	#return (str(rel_hum), str(amb_temp), str(probe_temp))
	return output
	
def get_ip():
    f = os.popen('ifconfig')
    for iface in [' '.join(i) for i in iter(lambda: list(itertools.takewhile(lambda l: not l.isspace(),f)), [])]:
        if re.findall('^(eth|wlan)[0-9]',iface) and re.findall('RUNNING',iface):
            ip = re.findall('(?<=inet\saddr:)[0-9\.]+',iface)
            if ip:
                return ip[0]
    return False
	
def updatePictureString( RelHum, AmbTemp, ProbeTemp, ip):
	TEXT = 'Hum=%s,Amb=%s,Probe=%s' % (str(RelHum), str(AmbTemp), str(ProbeTemp)) 
	print TEXT
	URL = 'http://' + ip + ':8080/0/config/set?text_left=' + TEXT
	print URL
	f = urllib2.urlopen(URL)
	print f.read()
	f.close()
	# http://192.168.1.26:8080/0/config/set?text_left=werwerwer

def takeSanpshot(ip):
	URL = 'http://' + ip + ':8080/0/action/snapshot'
	print URL 
	f = urllib2.urlopen(URL)
	print f.read()
	f.close()

def uploadToServer(Server, FtpUsername, FtpPassword):
	#ftp = FTP(Server, 21)
	n = "lastsnap"
	ftp = ftplib.FTP()
	ftp.connect(Server, "21")
	#print ftp.getwelcome()
	try:
	    ftp.login(FtpUsername, FtpPassword)
	    ftp.cwd("public_html/img/veggiecam")
	    f = open("./SNAPSHOTS/lastsnap.jpg", "rb")
	    name= str(n)+".jpg"
	    ftp.storbinary('STOR ' + name, f)
	    f.close()
	except:
	    traceback.print_exc()
	finally:
	    ftp.quit()    
	
# main() function
def main():
	PRIVATE_KEY = '35Y7G01PM35A0MRH'
	SERVER = 'veggiecam.net23.net'
	USERNAME = 'a3276289'
	PASSWORD = '6a4eb20e8426b0a7191fa3e5298e08ac51a794f42234d772ac177c96d4c78059'

    # use sys.argv if needed
    #if len(sys.argv) < 2:
    #    print('Usage: python tstest.py PRIVATE_KEY')
    #    exit(0)
	print 'starting...'
    
    # baseURL = 'https://api.thingspeak.com/update?api_key=%s' % sys.argv[1]
	baseURL = 'https://api.thingspeak.com/update?api_key=%s' % PRIVATE_KEY
	print 'base key'
	
	my_ip = get_ip()
	print my_ip
	
	while True:
		try:
			# RH, AT, PT = getSensorData()
			args = ("./thermo")
			popen = subprocess.Popen(args, stdout=subprocess.PIPE, shell=True)
			popen.wait()
			output = popen.stdout.read()
			print output
			#out = subprocess.check_output('./thermo')
			#print output
			#print 'RH= ', RH
			#print 'AT= ', AT 
			print 'New measurement'
			mylist = [float(x) for x in output.split(',')]
			print mylist
			if (mylist[0] != float(0) and mylist[2] != float(0)): # data sanity check
				f = urllib2.urlopen(baseURL + 
                                "&field1=%s&field2=%s&field3=%s" % (mylist[0], 
								mylist[1], mylist[2]))
				print f.read()
				f.close()
			
				updatePictureString(mylist[0], mylist[1], mylist[2], my_ip)
				takeSanpshot(my_ip)
				uploadToServer(SERVER, USERNAME, PASSWORD)
			sleep(60)
		except:
			print 'exiting.'
			break

# call main
if __name__ == '__main__':
    main()
