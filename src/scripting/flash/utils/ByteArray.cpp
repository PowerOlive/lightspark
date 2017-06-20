/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009-2013  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "scripting/abc.h"
#include "scripting/flash/utils/flashutils.h"
#include "asobject.h"
#include "scripting/class.h"
#include "compat.h"
#include "parsing/amf3_generator.h"
#include "scripting/argconv.h"
#include "scripting/flash/errors/flasherrors.h"
#include <sstream>
#include <zlib.h>
#include <glib.h>

using namespace std;
using namespace lightspark;

#define BA_CHUNK_SIZE 4096
// the flash documentation doesn't tell how large ByteArrays are allowed to be
// so we simply don't allow bytearrays larger than 1GiB
// maybe we should set this smaller
#define BA_MAX_SIZE 0x40000000

ByteArray::ByteArray(Class_base* c, uint8_t* b, uint32_t l):ASObject(c,T_OBJECT,SUBTYPE_BYTEARRAY),littleEndian(false),objectEncoding(ObjectEncoding::AMF3),currentObjectEncoding(ObjectEncoding::AMF3),
	position(0),bytes(b),real_len(l),len(l),shareable(false)
{
#ifdef MEMORY_USAGE_PROFILING
	c->memoryAccount->addBytes(l);
#endif
}

ByteArray::~ByteArray()
{
	if(bytes)
	{
#ifdef MEMORY_USAGE_PROFILING
		getClass()->memoryAccount->removeBytes(real_len);
#endif
		free(bytes);
	}
}

void ByteArray::sinit(Class_base* c)
{
	CLASS_SETUP(c, ASObject, _constructor, CLASS_SEALED);
	c->setDeclaredMethodByQName("length","",Class<IFunction>::getFunction(c->getSystemState(),_getLength),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("length","",Class<IFunction>::getFunction(c->getSystemState(),_setLength),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("bytesAvailable","",Class<IFunction>::getFunction(c->getSystemState(),_getBytesAvailable),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("position","",Class<IFunction>::getFunction(c->getSystemState(),_getPosition),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("position","",Class<IFunction>::getFunction(c->getSystemState(),_setPosition),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("endian","",Class<IFunction>::getFunction(c->getSystemState(),_getEndian),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("endian","",Class<IFunction>::getFunction(c->getSystemState(),_setEndian),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("objectEncoding","",Class<IFunction>::getFunction(c->getSystemState(),_getObjectEncoding),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("objectEncoding","",Class<IFunction>::getFunction(c->getSystemState(),_setObjectEncoding),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("defaultObjectEncoding","",Class<IFunction>::getFunction(c->getSystemState(),_getDefaultObjectEncoding),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("defaultObjectEncoding","",Class<IFunction>::getFunction(c->getSystemState(),_setDefaultObjectEncoding),SETTER_METHOD,false);

	c->getSystemState()->staticByteArrayDefaultObjectEncoding = ObjectEncoding::DEFAULT;
	c->setDeclaredMethodByQName("clear","",Class<IFunction>::getFunction(c->getSystemState(),clear),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("compress","",Class<IFunction>::getFunction(c->getSystemState(),_compress),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("uncompress","",Class<IFunction>::getFunction(c->getSystemState(),_uncompress),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("deflate","",Class<IFunction>::getFunction(c->getSystemState(),_deflate),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("inflate","",Class<IFunction>::getFunction(c->getSystemState(),_inflate),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readBoolean","",Class<IFunction>::getFunction(c->getSystemState(),readBoolean),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readBytes","",Class<IFunction>::getFunction(c->getSystemState(),readBytes),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readByte","",Class<IFunction>::getFunction(c->getSystemState(),readByte),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readDouble","",Class<IFunction>::getFunction(c->getSystemState(),readDouble),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readFloat","",Class<IFunction>::getFunction(c->getSystemState(),readFloat),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readInt","",Class<IFunction>::getFunction(c->getSystemState(),readInt),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readMultiByte","",Class<IFunction>::getFunction(c->getSystemState(),readMultiByte),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readShort","",Class<IFunction>::getFunction(c->getSystemState(),readShort),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readUnsignedByte","",Class<IFunction>::getFunction(c->getSystemState(),readUnsignedByte),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readUnsignedInt","",Class<IFunction>::getFunction(c->getSystemState(),readUnsignedInt),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readUnsignedShort","",Class<IFunction>::getFunction(c->getSystemState(),readUnsignedShort),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readObject","",Class<IFunction>::getFunction(c->getSystemState(),readObject),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readUTF","",Class<IFunction>::getFunction(c->getSystemState(),readUTF),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("readUTFBytes","",Class<IFunction>::getFunction(c->getSystemState(),readUTFBytes),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeBoolean","",Class<IFunction>::getFunction(c->getSystemState(),writeBoolean),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeUTF","",Class<IFunction>::getFunction(c->getSystemState(),writeUTF),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeUTFBytes","",Class<IFunction>::getFunction(c->getSystemState(),writeUTFBytes),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeBytes","",Class<IFunction>::getFunction(c->getSystemState(),writeBytes),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeByte","",Class<IFunction>::getFunction(c->getSystemState(),writeByte),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeDouble","",Class<IFunction>::getFunction(c->getSystemState(),writeDouble),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeFloat","",Class<IFunction>::getFunction(c->getSystemState(),writeFloat),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeInt","",Class<IFunction>::getFunction(c->getSystemState(),writeInt),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeMultiByte","",Class<IFunction>::getFunction(c->getSystemState(),writeMultiByte),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeUnsignedInt","",Class<IFunction>::getFunction(c->getSystemState(),writeUnsignedInt),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeObject","",Class<IFunction>::getFunction(c->getSystemState(),writeObject),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("writeShort","",Class<IFunction>::getFunction(c->getSystemState(),writeShort),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("toString","",Class<IFunction>::getFunction(c->getSystemState(),ByteArray::_toString),DYNAMIC_TRAIT);
	REGISTER_GETTER_SETTER(c,shareable);
	c->setDeclaredMethodByQName("atomicCompareAndSwapIntAt","",Class<IFunction>::getFunction(c->getSystemState(),atomicCompareAndSwapIntAt),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("atomicCompareAndSwapLength","",Class<IFunction>::getFunction(c->getSystemState(),atomicCompareAndSwapLength),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("toJSON",AS3,Class<IFunction>::getFunction(c->getSystemState(),_toJSON),DYNAMIC_TRAIT);

	c->addImplementedInterface(InterfaceClass<IDataInput>::getClass(c->getSystemState()));
	IDataInput::linkTraits(c);
	c->addImplementedInterface(InterfaceClass<IDataOutput>::getClass(c->getSystemState()));
	IDataOutput::linkTraits(c);
}

void ByteArray::buildTraits(ASObject* o)
{
}

void ByteArray::lock()
{
	if (shareable) mutex.lock();
}
void ByteArray::unlock()
{
	if (shareable) mutex.unlock();
}

uint8_t* ByteArray::getBuffer(unsigned int size, bool enableResize)
{
	if (size > BA_MAX_SIZE) 
		throwError<ASError>(kOutOfMemoryError);
	// The first allocation is exactly the size we need,
	// the subsequent reallocations happen in increments of BA_CHUNK_SIZE bytes
	uint32_t prevLen = len;
	if(bytes==NULL)
	{
		len=size;
		real_len=len;
		bytes = (uint8_t*) malloc(len);
#ifdef MEMORY_USAGE_PROFILING
		getClass()->memoryAccount->addBytes(len);
#endif
	}
	else if(enableResize==false)
	{
		assert_and_throw(size<=len);
	}
	else if(real_len<size) // && enableResize==true
	{
#ifdef MEMORY_USAGE_PROFILING
		uint32_t prev_real_len = real_len;
#endif
		while(real_len < size)
			real_len += BA_CHUNK_SIZE;
		// Reallocate the buffer, in chunks of BA_CHUNK_SIZE bytes
		uint8_t* bytes2 = (uint8_t*) realloc(bytes, real_len);
#ifdef MEMORY_USAGE_PROFILING
		getClass()->memoryAccount->addBytes(real_len-prev_real_len);
#endif
		assert_and_throw(bytes2);
		bytes = bytes2;
		len=size;
		bytes=bytes2;
	}
	else if(len<size)
	{
		len=size;
	}
	if(prevLen<size)
	{
		//Extend
		memset(bytes+prevLen,0,size-prevLen);
	}
	return bytes;
}

uint16_t ByteArray::endianIn(uint16_t value)
{
	if(littleEndian)
		return GUINT16_TO_LE(value);
	else
		return GUINT16_TO_BE(value);
}

uint32_t ByteArray::endianIn(uint32_t value)
{
	if(littleEndian)
		return GUINT32_TO_LE(value);
	else
		return GUINT32_TO_BE(value);
}

uint64_t ByteArray::endianIn(uint64_t value)
{
	if(littleEndian)
		return GUINT64_TO_LE(value);
	else
		return GUINT64_TO_BE(value);
}

uint16_t ByteArray::endianOut(uint16_t value)
{
	if(littleEndian)
		return GUINT16_FROM_LE(value);
	else
		return GUINT16_FROM_BE(value);
}

uint32_t ByteArray::endianOut(uint32_t value)
{
	if(littleEndian)
		return GUINT32_FROM_LE(value);
	else
		return GUINT32_FROM_BE(value);
}

uint64_t ByteArray::endianOut(uint64_t value)
{
	if(littleEndian)
		return GUINT64_FROM_LE(value);
	else
		return GUINT64_FROM_BE(value);
}

uint32_t ByteArray::getPosition() const
{
	return position;
}

ASFUNCTIONBODY(ByteArray,_constructor)
{
	return NULL;
}

ASFUNCTIONBODY(ByteArray,_getPosition)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	return abstract_i(obj->getSystemState(),th->getPosition());
}

void ByteArray::setPosition(uint32_t p)
{
	lock();
	position=p;
	unlock();
}

ASFUNCTIONBODY(ByteArray,_setPosition)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	uint32_t pos=args[0]->toUInt();
	th->setPosition(pos);
	return NULL;
}

ASFUNCTIONBODY(ByteArray,_getEndian)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	if(th->littleEndian)
		return abstract_s(obj->getSystemState(),Endian::littleEndian);
	else
		return abstract_s(obj->getSystemState(),Endian::bigEndian);
}

ASFUNCTIONBODY(ByteArray,_setEndian)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	if(args[0]->toString() == Endian::littleEndian)
		th->littleEndian = true;
	else if(args[0]->toString() == Endian::bigEndian)
		th->littleEndian = false;
	else
		throwError<ArgumentError>(kInvalidEnumError, "endian");
	return NULL;
}

ASFUNCTIONBODY(ByteArray,_getObjectEncoding)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	return abstract_ui(obj->getSystemState(),th->objectEncoding);
}

ASFUNCTIONBODY(ByteArray,_setObjectEncoding)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	uint32_t value;
	ARG_UNPACK(value);
	if(value!=ObjectEncoding::AMF0 && value!=ObjectEncoding::AMF3)
		throwError<ArgumentError>(kInvalidEnumError, "objectEncoding");

	th->objectEncoding=value;
	th->currentObjectEncoding=value;
	return NULL;
}

ASFUNCTIONBODY(ByteArray,_getDefaultObjectEncoding)
{
	return abstract_i(obj->getSystemState(),obj->getSystemState()->staticNetConnectionDefaultObjectEncoding);
}

ASFUNCTIONBODY(ByteArray,_setDefaultObjectEncoding)
{
	assert_and_throw(argslen == 1);
	int32_t value = args[0]->toInt();
	if(value == 0)
		args[0]->getSystemState()->staticByteArrayDefaultObjectEncoding = ObjectEncoding::AMF0;
	else if(value == 3)
		args[0]->getSystemState()->staticByteArrayDefaultObjectEncoding = ObjectEncoding::AMF3;
	else
		throw RunTimeException("Invalid object encoding");
	return NULL;
}

ASFUNCTIONBODY(ByteArray,_setLength)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==1);

	uint32_t newLen=args[0]->toInt();
	th->lock();
	if(newLen==th->len) //Nothing to do
		return NULL;
	th->setLength(newLen);
	th->unlock();
	return NULL;
}
void ByteArray::setLength(uint32_t newLen)
{
	if (newLen > 0)
	{
		getBuffer(newLen,true);
	}
	else
	{
		if (bytes)
		{
#ifdef MEMORY_USAGE_PROFILING
			getClass()->memoryAccount->removeBytes(th->real_len);
#endif
			free(bytes);
		}
		bytes = NULL;
		real_len = newLen;
	}
	len = newLen;
	if (position > len)
		position = (len > 0 ? len-1 : 0);
}
ASFUNCTIONBODY(ByteArray,_getLength)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	return abstract_i(obj->getSystemState(),th->len);
}

ASFUNCTIONBODY(ByteArray,_getBytesAvailable)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	return abstract_i(obj->getSystemState(),th->len-th->position);
}

ASFUNCTIONBODY(ByteArray,readBoolean)
{
	ByteArray* th=static_cast<ByteArray*>(obj);

	th->lock();
	uint8_t ret;
	if(!th->readByte(ret))
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}

	th->unlock();
	return abstract_b(obj->getSystemState(),ret!=0);
}

ASFUNCTIONBODY(ByteArray,readBytes)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	_NR<ByteArray> out;
	uint32_t offset;
	uint32_t length;
	ARG_UNPACK(out)(offset, 0)(length, 0);
	
	th->lock();
	if(length == 0)
	{
		assert(th->len >= th->position);
		length = th->len - th->position;
	}

	//Error checks
	if(th->position+length > th->len)
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}
	if((uint64_t)length+offset > 0xFFFFFFFF)
	{
		th->unlock();
		throw Class<RangeError>::getInstanceS(obj->getSystemState(),"length+offset");
	}
	
	uint8_t* buf=out->getBuffer(length+offset,true);
	memcpy(buf+offset,th->bytes+th->position,length);
	th->position+=length;
	th->unlock();

	return NULL;
}

bool ByteArray::readUTF(tiny_string& ret)
{
	uint16_t stringLen;
	if(!readShort(stringLen))
		return false;
	if(len < (position+stringLen))
		return false;
	// check for BOM
	if (len > position+3)
	{
		if (bytes[position] == 0xef &&
			bytes[position+1] == 0xbb &&
			bytes[position+2] == 0xbf)
		{
			position += 3;
			stringLen -= stringLen > 3 ? 3 : 0;
		}
	}
	char buf[stringLen+1];
	buf[stringLen]=0;
	strncpy(buf,(char*)bytes+position,(size_t)stringLen);
	ret=buf;
	position+=stringLen;
	return true;
}

ASFUNCTIONBODY(ByteArray,readUTF)
{
	ByteArray* th=static_cast<ByteArray*>(obj);

	tiny_string res;
	th->lock();
	if (!th->readUTF(res))
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}
	th->unlock();
	return abstract_s(obj->getSystemState(),res);
}

ASFUNCTIONBODY(ByteArray,readUTFBytes)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	uint32_t length;

	ARG_UNPACK (length);
	th->lock();
	if(th->position+length > th->len)
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}
	// check for BOM
	if (th->len > th->position+3)
	{
		if (th->bytes[th->position] == 0xef &&
			th->bytes[th->position+1] == 0xbb &&
			th->bytes[th->position+2] == 0xbf)
		{
			th->position += 3;
			length -=  length > 3 ? 3 : 0;
		}
	}

	uint8_t *bufStart=th->bytes+th->position;
	char buf[length+1];
	buf[length]=0;
	strncpy(buf,(char*)bufStart,(size_t)length);
	th->position+=length;
	th->unlock();
	return abstract_s(obj->getSystemState(),(char *)buf,strlen(buf));
}

void ByteArray::writeUTF(const tiny_string& str)
{
	getBuffer(position+str.numBytes()+2,true);
	if(str.numBytes() > 65535)
	{
		throwError<RangeError>(kParamRangeError);
	}
	uint16_t numBytes=endianIn((uint16_t)str.numBytes());
	memcpy(bytes+position,&numBytes,2);
	memcpy(bytes+position+2,str.raw_buf(),str.numBytes());
	position+=str.numBytes()+2;
}

ASFUNCTIONBODY(ByteArray,writeUTF)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	//Validate parameters
	assert_and_throw(argslen==1);
	assert_and_throw(args[0]->getObjectType()==T_STRING);
	ASString* str=Class<ASString>::cast(args[0]);
	th->lock();
	th->writeUTF(str->getData());
	th->unlock();
	return NULL;
}

ASFUNCTIONBODY(ByteArray,writeUTFBytes)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	//Validate parameters
	assert_and_throw(argslen==1);
	assert_and_throw(args[0]->getObjectType()==T_STRING);
	ASString* str=Class<ASString>::cast(args[0]);
	th->lock();
	th->getBuffer(th->position+str->getData().numBytes(),true);
	memcpy(th->bytes+th->position,str->getData().raw_buf(),str->getData().numBytes());
	th->position+=str->getData().numBytes();
	th->unlock();

	return NULL;
}

ASFUNCTIONBODY(ByteArray,writeMultiByte)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	tiny_string value;
	tiny_string charset;
	ARG_UNPACK(value)(charset);

	// TODO: should convert from UTF-8 to charset
	LOG(LOG_NOT_IMPLEMENTED, "ByteArray.writeMultiByte doesn't convert charset");

	th->lock();
	th->getBuffer(th->position+value.numBytes(),true);
	memcpy(th->bytes+th->position,value.raw_buf(),value.numBytes());
	th->position+=value.numBytes();
	th->unlock();

	return NULL;
}

uint32_t ByteArray::writeObject(ASObject* obj)
{
	//Return the length of the serialized object

	//TODO: support custom serialization
	map<tiny_string, uint32_t> stringMap;
	map<const ASObject*, uint32_t> objMap;
	map<const Class_base*, uint32_t> traitsMap;
	uint32_t oldPosition=position;
	obj->serialize(this, stringMap, objMap,traitsMap);
	return position-oldPosition;
}

ASFUNCTIONBODY(ByteArray,writeObject)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	//Validate parameters
	assert_and_throw(argslen==1);
	th->lock();
	th->writeObject(args[0]);
	th->unlock();

	return NULL;
}

void ByteArray::writeShort(uint16_t val)
{
	int16_t value2 = endianIn(val);
	getBuffer(position+2,true);
	memcpy(bytes+position,&value2,2);
	position+=2;
}

ASFUNCTIONBODY(ByteArray,writeShort)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	int32_t value;
	ARG_UNPACK(value);

	th->lock();
	th->writeShort((static_cast<uint16_t>(value & 0xffff)));
	th->unlock();
	return NULL;
}

ASFUNCTIONBODY(ByteArray,writeBytes)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	//Validate parameters
	assert_and_throw(argslen>=1 && argslen<=3);
	assert_and_throw(args[0]->getClass()->isSubClass(Class<ByteArray>::getClass(obj->getSystemState())));
	ByteArray* out=Class<ByteArray>::cast(args[0]);
	uint32_t offset=0;
	uint32_t length=0;
	if(argslen>=2)
		offset=args[1]->toUInt();
	if(argslen==3)
		length=args[2]->toUInt();

	// We need to clamp offset to the beginning of the bytes array
	if(offset > out->getLength()-1)
		offset = 0;
	// We need to clamp length to the end of the bytes array
	if(length > out->getLength()-offset)
		length = 0;

	//If the length is 0 the whole buffer must be copied
	if(length == 0)
		length=(out->getLength()-offset);
	uint8_t* buf=out->getBuffer(offset+length,false);
	th->lock();
	th->getBuffer(th->position+length,true);
	memcpy(th->bytes+th->position,buf+offset,length);
	th->position+=length;
	th->unlock();

	return NULL;
}

void ByteArray::writeByte(uint8_t b)
{
	getBuffer(position+1,true);
	bytes[position++] = b;
}

void ByteArray::writeBytes(uint8_t *data, int length)
{
	getBuffer(position+length,true);
	memcpy(bytes+position,data,length);
	position+=length;
}

ASFUNCTIONBODY(ByteArray,writeByte)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==1);

	int32_t value=args[0]->toInt();

	th->lock();
	th->writeByte(value&0xff);
	th->unlock();

	return NULL;
}

ASFUNCTIONBODY(ByteArray,writeBoolean)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	bool b;
	ARG_UNPACK (b);

	th->lock();
	if (b)
		th->writeByte(1);
	else
		th->writeByte(0);
	th->unlock();

	return NULL;
}

ASFUNCTIONBODY(ByteArray,writeDouble)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==1);

	double value = args[0]->toNumber();
	uint64_t *intptr=reinterpret_cast<uint64_t*>(&value);
	uint64_t value2=th->endianIn(*intptr);

	th->lock();
	th->getBuffer(th->position+8,true);
	memcpy(th->bytes+th->position,&value2,8);
	th->position+=8;
	th->unlock();

	return NULL;
}

ASFUNCTIONBODY(ByteArray,writeFloat)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==1);

	float value = args[0]->toNumber();
	uint32_t *intptr=reinterpret_cast<uint32_t*>(&value);
	uint32_t value2=th->endianIn(*intptr);

	th->lock();
	th->getBuffer(th->position+4,true);
	memcpy(th->bytes+th->position,&value2,4);
	th->position+=4;
	th->unlock();

	return NULL;
}

ASFUNCTIONBODY(ByteArray,writeInt)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==1);

	uint32_t value=th->endianIn(static_cast<uint32_t>(args[0]->toInt()));

	th->lock();
	th->getBuffer(th->position+4,true);
	memcpy(th->bytes+th->position,&value,4);
	th->position+=4;
	th->unlock();

	return NULL;
}

void ByteArray::writeUnsignedInt(uint32_t val)
{
	getBuffer(position+4,true);
	memcpy(bytes+position,&val,4);
	position+=4;
}

ASFUNCTIONBODY(ByteArray,writeUnsignedInt)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==1);

	th->lock();
	uint32_t value=th->endianIn(args[0]->toUInt());
	th->writeUnsignedInt(value);
	th->unlock();
	return NULL;
}

bool ByteArray::peekByte(uint8_t& b)
{
	if (len <= position+1)
		return false;

	b=bytes[position+1];
	return true;
}
bool ByteArray::readByte(uint8_t& b)
{
	if (len <= position)
		return false;

	b=bytes[position++];
	return true;
}

bool ByteArray::readU29(uint32_t& ret)
{
	//Be careful! This is different from u32 parsing.
	//Here the most significant bits appears before in the stream!
	ret=0;
	for(uint32_t i=0;i<4;i++)
	{
		if (len <= position)
			return false;

		uint8_t tmp=bytes[position++];
		ret <<= 7;
		if(i<3)
		{
			ret |= tmp&0x7f;
			if((tmp&0x80)==0)
				break;
		}
		else
		{
			ret |= tmp;
			//Sign extend
			if(tmp&0x80)
				ret|=0xe0000000;
		}
	}
	return true;
}

ASFUNCTIONBODY(ByteArray, readByte)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==0);

	th->lock();
	uint8_t ret;
	if(!th->readByte(ret))
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}
	th->unlock();
	return abstract_i(obj->getSystemState(),(int8_t)ret);
}

ASFUNCTIONBODY(ByteArray,readDouble)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==0);

	th->lock();
	if(th->len < th->position+8)
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}

	uint64_t ret;
	memcpy(&ret,th->bytes+th->position,8);
	th->position+=8;
	ret = th->endianOut(ret);

	double *doubleptr=reinterpret_cast<double*>(&ret);
	th->unlock();
	return abstract_d(obj->getSystemState(),*doubleptr);
}

ASFUNCTIONBODY(ByteArray,readFloat)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==0);

	th->lock();
	if(th->len < th->position+4)
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}

	uint32_t ret;
	memcpy(&ret,th->bytes+th->position,4);
	th->position+=4;
	ret = th->endianOut(ret);

	float *floatptr=reinterpret_cast<float*>(&ret);
	th->unlock();
	return abstract_d(obj->getSystemState(),*floatptr);
}

ASFUNCTIONBODY(ByteArray,readInt)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==0);

	th->lock();
	if(th->len < th->position+4)
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}

	uint32_t ret;
	memcpy(&ret,th->bytes+th->position,4);
	th->position+=4;
	th->unlock();
	return abstract_i(obj->getSystemState(),(int32_t)th->endianOut(ret));
}

bool ByteArray::readShort(uint16_t& ret)
{
	if (len < position+2)
		return false;

	uint16_t tmp;
	memcpy(&tmp,bytes+position,2);
	ret=endianOut(tmp);
	position+=2;
	return true;
}

ASFUNCTIONBODY(ByteArray,readShort)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==0);

	uint16_t ret;
	th->lock();
	if(!th->readShort(ret))
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}

	th->unlock();
	return abstract_i(obj->getSystemState(),(int16_t)ret);
}

ASFUNCTIONBODY(ByteArray,readUnsignedByte)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==0);

	uint8_t ret;
	th->lock();
	if (!th->readByte(ret))
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}
	return abstract_ui(obj->getSystemState(),ret);
}

bool ByteArray::readUnsignedInt(uint32_t& ret)
{
	if(len < position+4)
		return false;

	uint32_t tmp;
	memcpy(&tmp,bytes+position,4);
	ret=endianOut(tmp);
	position+=4;
	return true;
}

ASFUNCTIONBODY(ByteArray,readUnsignedInt)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==0);

	uint32_t ret;
	th->lock();
	if(!th->readUnsignedInt(ret))
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}
	th->unlock();
	return abstract_ui(obj->getSystemState(),ret);
}

ASFUNCTIONBODY(ByteArray,readUnsignedShort)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==0);

	uint16_t ret;
	th->lock();
	if(!th->readShort(ret))
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}

	return abstract_ui(obj->getSystemState(),ret);
}

ASFUNCTIONBODY(ByteArray,readMultiByte)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	uint32_t strlen;
	tiny_string charset;
	ARG_UNPACK(strlen)(charset);

	th->lock();
	if(th->len < th->position+strlen)
	{
		th->unlock();
		throwError<EOFError>(kEOFError);
	}

	// TODO: should convert from charset to UTF-8
	LOG(LOG_NOT_IMPLEMENTED, "ByteArray.readMultiByte doesn't convert charset");
	return abstract_s(obj->getSystemState(),(char*)th->bytes+th->position,strlen);
}

ASFUNCTIONBODY(ByteArray,readObject)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	assert_and_throw(argslen==0);
	th->lock();
	if(th->bytes==NULL)
	{
		th->unlock();
		// it seems that contrary to the specs Adobe returns Undefined when reading from an empty ByteArray
		return obj->getSystemState()->getUndefinedRef();
		//throwError<EOFError>(kEOFError);
	}
	//assert_and_throw(th->objectEncoding==ObjectEncoding::AMF3);
	Amf3Deserializer d(th);
	_NR<ASObject> ret(NullRef);
	try
	{
		ret=d.readObject();
		th->unlock();
	}
	catch(LightsparkException& e)
	{
		th->unlock();
		LOG(LOG_ERROR,"Exception caught while parsing AMF3: " << e.cause);
		//TODO: throw AS exception
	}

	if(ret.isNull())
	{
		LOG(LOG_ERROR,"No objects in the AMF3 data. Returning Undefined");
		return obj->getSystemState()->getUndefinedRef();
	}
	ret->incRef();
	return ret.getPtr();
}

ASFUNCTIONBODY(ByteArray,_toString)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	//check for Byte Order Mark
	int start = 0;
	if (th->len > 3)
	{
		if (th->bytes[0] == 0xef &&
			th->bytes[1] == 0xbb &&
			th->bytes[2] == 0xbf)
			start = 3;
	}
	return abstract_s(obj->getSystemState(),(char*)th->bytes+start,th->len-start);
}

bool ByteArray::hasPropertyByMultiname(const multiname& name, bool considerDynamic, bool considerPrototype)
{
	if(considerDynamic==false)
		return ASObject::hasPropertyByMultiname(name, considerDynamic, considerPrototype);
	if (!isConstructed())
		return false;

	unsigned int index=0;
	if(!Array::isValidMultiname(getSystemState(),name,index))
		return ASObject::hasPropertyByMultiname(name, considerDynamic, considerPrototype);

	return index<len;
}

asAtom ByteArray::getVariableByMultiname(const multiname& name, GET_VARIABLE_OPTION opt)
{
	unsigned int index=0;
	if((opt & ASObject::SKIP_IMPL)!=0  || !implEnable || !Array::isValidMultiname(getSystemState(),name,index))
		return ASObject::getVariableByMultiname(name,opt);

	if(index<len)
	{
		uint8_t value = bytes[index];
		return asAtom(static_cast<uint32_t>(value));
	}
	else
		return asAtom::undefinedAtom;
}

int32_t ByteArray::getVariableByMultiname_i(const multiname& name)
{
	assert_and_throw(implEnable);
	unsigned int index=0;
	if(!Array::isValidMultiname(getSystemState(),name,index))
		return ASObject::getVariableByMultiname_i(name);

	if(index<len)
	{
		uint8_t value = bytes[index];
		return static_cast<uint32_t>(value);
	}
	else
		return _MNR(getSystemState()->getUndefinedRef());
}

void ByteArray::setVariableByMultiname(const multiname& name, asAtom& o, CONST_ALLOWED_FLAG allowConst)
{
	assert_and_throw(implEnable);
	unsigned int index=0;
	if(!Array::isValidMultiname(getSystemState(),name,index))
		return ASObject::setVariableByMultiname(name,o,allowConst);
	if (index > BA_MAX_SIZE) 
		throwError<ASError>(kOutOfMemoryError);

	if(index>=len)
	{
		uint32_t prevLen = len;
		getBuffer(index+1, true);
		// Fill the gap between the end of the current data and the index with zeros
		memset(bytes+prevLen, 0, index-prevLen);
	}

	// Fill the byte pointed to by index with the truncated uint value of the object.
	uint8_t value = static_cast<uint8_t>(o.toUInt() & 0xff);
	bytes[index] = value;

	ASATOM_DECREF(o);
}

void ByteArray::setVariableByMultiname_i(const multiname& name, int32_t value)
{
	asAtom v = asAtom(value);
	setVariableByMultiname(name, v,ASObject::CONST_NOT_ALLOWED);
}

void ByteArray::acquireBuffer(uint8_t* buf, int bufLen)
{
	if(bytes)
	{
#ifdef MEMORY_USAGE_PROFILING
		getClass()->memoryAccount->removeBytes(real_len);
#endif
		free(bytes);
	}
	bytes=buf;
	real_len=bufLen;
	len=bufLen;
#ifdef MEMORY_USAGE_PROFILING
	getClass()->memoryAccount->addBytes(real_len);
#endif
	position=0;
}

void ByteArray::writeU29(uint32_t val)
{
	for(uint32_t i=0;i<4;i++)
	{
		uint8_t b;
		if(i<3)
		{
			uint32_t tmp=(val >> ((3-i)*7));
			if(tmp==0)
				continue;

			b=(tmp&0x7f)|0x80;
		}
		else
			b=val&0x7f;

		writeByte(b);
	}
}

void ByteArray::serializeDouble(number_t val)
{
	//We have to write the double in network byte order (big endian)
	const uint64_t* tmpPtr=reinterpret_cast<const uint64_t*>(&val);
	uint64_t bigEndianVal=GINT64_FROM_BE(*tmpPtr);
	uint8_t* bigEndianPtr=reinterpret_cast<uint8_t*>(&bigEndianVal);
		
	for(uint32_t i=0;i<8;i++)
		writeByte(bigEndianPtr[i]);
	
}

void ByteArray::writeStringVR(map<tiny_string, uint32_t>& stringMap, const tiny_string& s)
{
	const uint32_t len=s.numBytes();
	if(len >= 1<<28)
		throwError<RangeError>(kParamRangeError);

	//Check if the string is already in the map
	auto it=stringMap.find(s);
	if(it!=stringMap.end())
	{
		//The first bit must be 0, the next 29 bits
		//store the index of the string in the map
		writeU29(it->second << 1);
	}
	else
	{
		//The AMF3 spec says that the empty string is never sent by reference
		//So add the string to the map only if it's not the empty string
		if(len)
			stringMap.insert(make_pair(s, stringMap.size()));

		//The first bit must be 1, the next 29 bits
		//store the number of bytes of the string
		writeU29((len<<1) | 1);

		getBuffer(position+len,true);
		memcpy(bytes+position,s.raw_buf(),len);
		position+=len;
	}
}

void ByteArray::writeStringAMF0(const tiny_string& s)
{
	const uint32_t len=s.numBytes();
	if(len <= 0xffff)
	{
		writeUTF(s);
	}
	else
	{
		getBuffer(position+len+4,true);
		uint32_t numBytes=endianIn((uint32_t)len);
		memcpy(bytes+position,&numBytes,4);
		memcpy(bytes+position+4,s.raw_buf(),len);
		position+=len+4;
	}
}

void ByteArray::writeXMLString(std::map<const ASObject*, uint32_t>& objMap,
			       ASObject *xml,
			       const tiny_string& xmlstr)
{
	if(xmlstr.numBytes() >= 1<<28)
		throwError<RangeError>(kParamRangeError);

	//Check if the XML object has been already serialized
	auto it=objMap.find(xml);
	if(it!=objMap.end())
	{
		//The least significant bit is 0 to signal a reference
		writeU29(it->second << 1);
	}
	else
	{
		//Add the XML object to the map
		objMap.insert(make_pair(xml, objMap.size()));

		//The first bit must be 1, the next 29 bits
		//store the number of bytes of the string
		writeU29((xmlstr.numBytes()<<1) | 1);

		getBuffer(position+xmlstr.numBytes(),true);
		memcpy(bytes+position,xmlstr.raw_buf(),xmlstr.numBytes());
		position+=xmlstr.numBytes();
	}
}
void ByteArray::append(streambuf *data, int length)
{
	lock();
	int oldlen = len;
	getBuffer(len+length,true);
	istream s(data);
	s.read((char*)bytes+oldlen,length);
	unlock();
}
void ByteArray::removeFrontBytes(int count)
{
	memmove(bytes,bytes+count,count);
	position -= count;
	len -= count;
}



void ByteArray::compress_zlib()
{
	if(len==0)
		return;

	unsigned long buflen=compressBound(len);
	uint8_t *compressed=(uint8_t*) malloc(buflen);
	assert_and_throw(compressed);

	if(compress(compressed, &buflen, bytes, len)!=Z_OK)
	{
		free(compressed);
		throw RunTimeException("zlib compress failed");
	}

	acquireBuffer(compressed, buflen);
	position=buflen;
}

void ByteArray::uncompress_zlib()
{
	z_stream strm;
	int status;

	if(len==0)
		return;

	strm.zalloc=Z_NULL;
	strm.zfree=Z_NULL;
	strm.opaque=Z_NULL;
	strm.avail_in=len;
	strm.next_in=bytes;
	strm.total_out=0;
	status=inflateInit(&strm);
	if(status==Z_VERSION_ERROR)
		throw Class<IOError>::getInstanceS(getSystemState(),"not valid compressed data");
	else if(status!=Z_OK)
		throw RunTimeException("zlib uncompress failed");

	vector<uint8_t> buf(3*len);
	do
	{
		strm.next_out=&buf[strm.total_out];
		strm.avail_out=buf.size()-strm.total_out;
		status=inflate(&strm, Z_NO_FLUSH);

		if(status!=Z_OK && status!=Z_STREAM_END)
		{
			inflateEnd(&strm);
			throw Class<IOError>::getInstanceS(getSystemState(),"not valid compressed data");
		}

		if(strm.avail_out==0)
			buf.resize(buf.size()+len);
	} while(status!=Z_STREAM_END);

	inflateEnd(&strm);

	len=strm.total_out;
#ifdef MEMORY_USAGE_PROFILING
	getClass()->memoryAccount->addBytes(len-real_len);
#endif
	real_len = len;
	uint8_t* bytes2=(uint8_t*) realloc(bytes, len);
	assert_and_throw(bytes2);
	bytes = bytes2;
	memcpy(bytes, &buf[0], len);
	position=0;
}

ASFUNCTIONBODY(ByteArray,_compress)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	// flash throws an error if compress is called with a compression algorithm,
	// and always uses the zlib algorithm
	// but tamarin tests do not catch it, so we simply ignore any parameters provided
	th->lock();
	th->compress_zlib();
	th->unlock();
	return NULL;
}

ASFUNCTIONBODY(ByteArray,_uncompress)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	// flash throws an error if uncompress is called with a compression algorithm,
	// and always uses the zlib algorithm
	// but tamarin tests do not catch it, so we simply ignore any parameters provided
	th->lock();
	th->uncompress_zlib();
	th->unlock();
	return NULL;
}

ASFUNCTIONBODY(ByteArray,_deflate)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	th->lock();
	th->compress_zlib();
	th->unlock();
	return NULL;
}

ASFUNCTIONBODY(ByteArray,_inflate)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	th->lock();
	th->uncompress_zlib();
	th->unlock();
	return NULL;
}

ASFUNCTIONBODY(ByteArray,clear)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	th->lock();
	if(th->bytes)
	{
#ifdef MEMORY_USAGE_PROFILING
		th->getClass()->memoryAccount->removeBytes(th->real_len);
#endif
		free(th->bytes);
	}
	th->bytes = NULL;
	th->len=0;
	th->real_len=0;
	th->position=0;
	th->unlock();
	return NULL;
}

// this seems to be how AS3 handles generic pop calls in Array class
ASFUNCTIONBODY_ATOM(ByteArray,pop)
{
	ByteArray* th=obj.as<ByteArray>();
	uint8_t res = 0;
	th->lock();
	if (th->readByte(res))
	{
		memmove(th->bytes,(th->bytes+1),th->getLength()-1);
		th->len--;
	}
	th->unlock();
	return asAtom((uint32_t)res);
	
}

// this seems to be how AS3 handles generic push calls in Array class
ASFUNCTIONBODY_ATOM(ByteArray,push)
{
	ByteArray* th=static_cast<ByteArray*>(obj.getObject());
	th->lock();
	th->getBuffer(th->len+argslen,true);
	for (unsigned int i = 0; i < argslen; i++)
	{
		th->bytes[th->len+i] = (uint8_t)args[i].toInt();
	}
	uint32_t res = th->getLength();
	th->unlock();
	return asAtom(res);
}

// this seems to be how AS3 handles generic shift calls in Array class
ASFUNCTIONBODY_ATOM(ByteArray,shift)
{
	ByteArray* th=obj.as<ByteArray>();
	uint8_t res = 0;
	th->lock();
	if (th->readByte(res))
	{
		memmove(th->bytes,(th->bytes+1),th->getLength()-1);
		th->len--;
	}
	th->unlock();
	return asAtom((uint32_t)res);
}

// this seems to be how AS3 handles generic unshift calls in Array class
ASFUNCTIONBODY_ATOM(ByteArray,unshift)
{
	ByteArray* th=obj.as<ByteArray>();
	th->lock();
	th->getBuffer(th->len+argslen,true);
	for (unsigned int i = 0; i < argslen; i++)
	{
		memmove((th->bytes+argslen),(th->bytes),th->len);
		th->bytes[i] = (uint8_t)args[i].toInt();
	}
	uint32_t res = th->getLength();
	th->unlock();
	return asAtom(res);
}
ASFUNCTIONBODY_GETTER_SETTER(ByteArray,shareable);

ASFUNCTIONBODY(ByteArray,atomicCompareAndSwapIntAt)
{
	ByteArray* th=static_cast<ByteArray*>(obj);

	int32_t byteindex,expectedValue,newvalue;
	ARG_UNPACK(byteindex)(expectedValue)(newvalue);

	if (byteindex < 0 || byteindex%4)
	{
		throwError<RangeError>(kInvalidRangeError, obj->getClassName());
	}
	th->lock();
	if(byteindex >= (int32_t)th->len-4)
	{
		th->unlock();
		throwError<RangeError>(kInvalidRangeError, obj->getClassName());
	}
	int32_t ret;
	memcpy(&ret,th->bytes+byteindex,4);

	if (ret == expectedValue)
	{
		memcpy(th->bytes+byteindex,&newvalue,4);
	}
	th->unlock();
	return abstract_i(obj->getSystemState(),ret);
}
ASFUNCTIONBODY(ByteArray,atomicCompareAndSwapLength)
{
	ByteArray* th=static_cast<ByteArray*>(obj);
	int32_t expectedLength,newLength;
	ARG_UNPACK(expectedLength)(newLength);

	th->lock();
	int32_t ret = th->len;
	if (ret == expectedLength)
	{
		th->setLength(newLength);
	}
	th->unlock();
	return abstract_i(obj->getSystemState(),ret);
}

ASFUNCTIONBODY(ByteArray,_toJSON)
{
	return abstract_s(getSys(),"ByteArray");
}

void ByteArray::serialize(ByteArray* out, std::map<tiny_string, uint32_t>& stringMap,
				std::map<const ASObject*, uint32_t>& objMap,
				std::map<const Class_base*, uint32_t>& traitsMap)
{
	if (out->getObjectEncoding() == ObjectEncoding::AMF0)
	{
		LOG(LOG_NOT_IMPLEMENTED,"serializing ByteArray in AMF0 not implemented");
		return;
	}
	assert_and_throw(objMap.find(this)==objMap.end());
	out->writeByte(byte_array_marker);
	//Check if the bytearray has been already serialized
	auto it=objMap.find(this);
	if(it!=objMap.end())
	{
		//The least significant bit is 0 to signal a reference
		out->writeU29(it->second << 1);
	}
	else
	{
		//Add the dictionary to the map
		objMap.insert(make_pair(this, objMap.size()));

		assert_and_throw(len<0x20000000);
		uint32_t value = (len << 1) | 1;
		out->writeU29(value);
		// TODO faster implementation
		for (unsigned int i = 0; i < len; i++)
			out->writeByte(this->bytes[i]);
	}
}
