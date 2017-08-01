"""
 *  Copyright (C) 2017, IOActive www.ioactive.com
 *
 *  This file is part of Go Nuclear: Breaking Radiation Monitoring Devices - BlackHat'17
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
"""

from Crypto.Cipher import DES3
import sys, getopt,os


ifile=''
ofile=''
operation=0

myopts, args = getopt.getopt(sys.argv[1:],"i:o:e")
 

for o, a in myopts:
    if o == '-i':
        ifile=a
    elif o == '-o':
        ofile=a
    elif o == '-e':
    	operation=1
    else:
        print("Usage: %s [oper, -d , -e] -i input -o output" % sys.argv[0])
        quit(0)
 
print ("Input file : %s and output file: %s" % (ifile,ofile) )

key = "B7E648AE72434579B7F4D482587075D2B7E648AE72434579".decode('hex') #PASS: "!12dfGPXT" a.class
iv = "B7E648AE72434579".decode('hex')
des3 = DES3.new(key,DES3.MODE_CBC,iv)

fIn = open(ifile,'rb')
fOut = open(ofile,'wb')

dataIn = fIn.read()

if operation == 1:
	fOut.write(des3.encrypt(dataIn))
else:
	fOut.write(des3.decrypt(dataIn))

fOut.close()
fIn.close()

