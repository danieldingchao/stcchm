import hashlib   
import sys

def encrypt(input_file,out_file):
    file_object = open(input_file)
    input = ''
    try:
        input = file_object.read( )
    finally:
        file_object.close( )

    m2 = hashlib.md5()   
    m2.update(input)   
    hash = m2.hexdigest()  
    
    result=[]
    index = 0
    for i in input:
        result.append(chr(ord(i)^(index+9)%13 + ord('B')))
        index = index + 1
    result.append(hash)    
    file_object = open(out_file, 'wb')
    file_object.write(''.join(result))
    file_object.close( )

if len(sys.argv)<2:
    print "please input Input File and Output file"
else:
    print sys.argv[1]
    encrypt(sys.argv[1],sys.argv[2])


