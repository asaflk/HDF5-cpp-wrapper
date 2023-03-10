/*
Copyright	(c)	2015	Michael	Welter


Permission	is	hereby	granted,	free	of	charge,	to	any	person	obtaining	a	copy
of	this	software	and	associated	documentation	files	(the	"Software"),	to	deal
in	the	Software	without	restriction,	including	without	limitation	the	rights
to	use,	copy,	modify,	merge,	publish,	distribute,	sublicense,	and/or	sell
copies	of	the	Software,	and	to	permit	persons	to	whom	the	Software	is
furnished	to	do	so,	subject	to	the	following	conditions:


The	above	copyright	notice	and	this	permission	notice	shall	be	included	in
all	copies	or	substantial	portions	of	the	Software.


THE	SOFTWARE	IS	PROVIDED	"AS	IS",	WITHOUT	WARRANTY	OF	ANY	KIND,	EXPRESS	OR
IMPLIED,	INCLUDING	BUT	NOT	LIMITED	TO	THE	WARRANTIES	OF	MERCHANTABILITY,
FITNESS	FOR	A	PARTICULAR	PURPOSE	AND	NONINFRINGEMENT.	IN	NO	EVENT	SHALL	THE
AUTHORS	OR	COPYRIGHT	HOLDERS	BE	LIABLE	FOR	ANY	CLAIM,	DAMAGES	OR	OTHER
LIABILITY,	WHETHER	IN	AN	ACTION	OF	CONTRACT,	TORT	OR	OTHERWISE,	ARISING	FROM,
OUT	OF	OR	IN	CONNECTION	WITH	THE	SOFTWARE	OR	THE	USE	OR	OTHER	DEALINGS	IN
THE	SOFTWARE.
*/

#ifndef	_HDF_WRAPPER_H
#define	_HDF_WRAPPER_H

#include	<exception>
#include	<iostream>	//	for	dealing	with	strings	in	exceptions,	mostly
#include	<string>//	for	dealing	with	strings	in	exceptions,	mostly
#include	<sstream>//	for	dealing	with	strings	in	exceptions,	mostly
#include	<type_traits>	//	for	removal	of	const	qualifiers
#include	<limits>

#include	<assert.h>
#include	<vector>
#include	<iterator>

#if	(defined	__APPLE__)
//	implement	nice	exception	messages	that	need	string	manipulation
#elif	(defined	_MSC_VER)
#include	<stdio.h>	//	for	FILE	stream	manipulation
#pragma	warning	(disable	:	4800)	//	performance	warning	for	conversion	to	bool
#elif	(defined	__GNUG__)
#include	<stdio.h>
#endif

#include	"hdf5.h"

#ifdef	HDF_WRAPPER_HAS_BOOST
#include	<boost/optional.hpp>
#endif

namespace	h5cpp
{

class	Object;
class	Dataspace;
class	Dataset;
class	Datatype;
class	Attributes;
class	Attribute;
class	File;
class	Group;


namespace	internal
{
struct	TagOpen	{};
struct	TagCreate	{};
struct	IncRC	{};
struct	NoIncRC	{};
}

static	void	disableAutoErrorReporting()
{
H5Eset_auto(H5E_DEFAULT,	NULL,	NULL);
};

class	AutoErrorReportingGuard
{
void	*client_data;
H5E_auto2_t	func;
public:
AutoErrorReportingGuard()
{
H5Eget_auto2(H5E_DEFAULT,	&func,	&client_data);
}
void	disableReporting()
{
H5Eset_auto2(H5E_DEFAULT,	NULL,	NULL);
}
~AutoErrorReportingGuard()
{
H5Eset_auto2(H5E_DEFAULT,	func,	client_data);
}
};

namespace	internal
{
/*
hack	around	a	strange	issue:	err_desc	is	partially	filled	with	garbage	(func_name,	file_name,	desc).	Therefore,
this	custom	error	printer	is	used.
*/
static	herr_t	custom_print_cb(unsigned	n,	const	H5E_error2_t	*err_desc,	void*	client_data)
{
const	int	MSG_SIZE	=	256;
auto	*out	=	static_cast<std::string*>(client_data);
char	maj[MSG_SIZE];
char	min[MSG_SIZE];
char	cls[MSG_SIZE];
char	buffer[MSG_SIZE];

/*	Get	descriptions	for	the	major	and	minor	error	numbers	*/
if	(H5Eget_class_name(err_desc->cls_id,	cls,	MSG_SIZE)	<	0)
return	-1;

if	(H5Eget_msg(err_desc->maj_num,	NULL,	maj,	MSG_SIZE)<0)
return	-1;

if	(H5Eget_msg(err_desc->min_num,	NULL,	min,	MSG_SIZE)<0)
return	-1;

const	char*	fmt	=	"	*	%s	%s:	%s";
#if	(defined	__APPLE__)	//	not	even	tested	to	compile	this
snprintf(buffer,	MSG_SIZE,	fmt,	cls,	maj,	min);
#elif	(defined	_MSC_VER)
sprintf_s(buffer,	MSG_SIZE,	fmt,	cls,	maj,	min);
#elif	(defined	__GNUG__)
snprintf(buffer,	MSG_SIZE,	fmt,	cls,	maj,	min);
#endif
buffer[MSG_SIZE	-	1]	=	0;	//	its	the	only	way	to	be	sure	...
try
{
out->append(buffer);
}
catch	(...)
{
//	suppress	all	exceptions	since	thsi	function	is	called	from	a	c	library.
}
return	0;
}

}	//	namespace	internal

class	Exception	:	public	std::exception
{
std::string	msg;
public:
Exception(const	std::string	&msg_)	:	msg(msg_)
{
#if	(defined	__APPLE__)
//	implement	nice	exception	messages	that	need	string	manipulation
#elif	(defined	_MSC_VER)	||	(defined	__GNUG__)
msg.append(".	Error	Stack:");
H5Ewalk2(H5E_DEFAULT,	H5E_WALK_DOWNWARD,	&internal::custom_print_cb,	&msg);
#endif
}
Exception()	:	msg("Unspecified	error")	{	assert(false);	}
~Exception()	throw()	{}
const	char*	what()	const	throw()	{	return	msg.c_str();	}
};

class	NameLookupError	:	public	Exception
{
public:
NameLookupError(const	std::string	&name)	:	Exception("Cannot	find	'"+name+"'")	{}
};



//	definitions	are	at	the	end	of	the	file
template<class	T>
inline	Datatype	get_disktype();

template<class	T>
inline	Datatype	get_memtype();


class	Object
{
void	check_valid_throw()
{
htri_t	ok	=	H5Iis_valid(id);
if	(!ok)
throw	Exception("initialization	of	Object	with	invalid	handle");
}

public:
Object(const	Object	&o)	:	id(o.id)
{
inc_ref();
}

virtual	~Object()
{
dec_ref();
}

Object&	operator=(const	Object	&o)
{
if	(id	==	o.id)	return	*this;
this->~Object();
id	=	o.id;
inc_ref();
return	*this;
}

hid_t	get_id()	const
{
return	id;
}

Object()	:	id(-1)	{}

//	normally	we	get	a	new	handle.	Its	count	needs	only	decreasing.
Object(hid_t	id)	:	id(id)
{
check_valid_throw();
}

Object(hid_t	id,	internal::IncRC)	:	id(id)
{
check_valid_throw();
inc_ref();
}

std::string	get_name()	const
{
std::string	res;
ssize_t	l	=	H5Iget_name(id,	NULL,	0);
if	(l	<	0)
throw	Exception("cannot	get	object	name");
res.resize(l);
if	(l>0)
H5Iget_name(id,	&res[0],	l+1);
return	res;
}

std::string	get_file_name()	const
{
std::string	res;
ssize_t	l	=	H5Fget_name(id,	NULL,	0);
if	(l	<	0)
throw	Exception("cannot	get	object	file	name");
res.resize(l);
if	(l>0)
H5Fget_name(id,	&res[0],	l+1);
return	res;
}

inline	File	get_file()	const;

bool	is_valid()	const
{
return	H5Iis_valid(id)	>	0;
}

//	references	the	same	object	as	other
bool	is_same(const	Object	&other)	const
{
//	just	need	to	compare	id's
return	other.id	==	this->id;
}

void	inc_ref()
{
if	(id	<	0)	return;
int	r	=	H5Iinc_ref(id);
if	(r	<	0)
throw	Exception("error	inc	ref	count");
}

void	dec_ref()
{
if	(id	<	0)	return;
int	r	=	H5Idec_ref(id);
if	(r	<	0)
throw	Exception("error	dec	ref	count");
if	(H5Iis_valid(id)	>	0)
id	=	-1;
}

int	get_ref()
{
if	(id	<	0)	return	0;
int	r	=	H5Iget_ref(id);
if	(r	<	0)
throw	Exception("error	getting	ref	count");
return	r;
}

protected:
hid_t	id;
};


class	Datatype	:	public	Object
{
private:
friend	class	Attribute;
friend	class	Dataset;

public:
Datatype(hid_t	id)	:	Object(id)	{}
Datatype(hid_t	id,	internal::IncRC)	:	Object(id,	internal::IncRC())	{}
Datatype()	:	Object()	{}

static	Datatype	copy(hid_t	id)
{
hid_t	newid	=	H5Tcopy(id);
if	(newid	<	0)
throw	Exception("error	copying	datatype");
return	Datatype(newid);
}

static	Datatype	createArray(const	Datatype	&base,	int	ndims,	int	*dims)
{
hsize_t	hdims[H5S_MAX_RANK];
for	(int	i=0;	i<ndims;	++i)	hdims[i]	=	dims[i];
hid_t	id	=	H5Tarray_create2(base.get_id(),	ndims,	hdims);
if	(id	<	0)
throw	Exception("error	creating	array	data	type");
return	Datatype(id);
}

void	set_size(size_t	s)
{
herr_t	err	=	H5Tset_size(this->id,	s);
if	(err	<	0)
throw	Exception("cannot	set	datatype	size");
}

void	set_variable_size()	{	set_size(H5T_VARIABLE);	}

size_t	get_size()	//	in	bytes
{
size_t	s	=	H5Tget_size(this->id);
if	(s	==	0)
throw	Exception("cannot	get	datatype	size");
return	s;
}

bool	is_equal(const	Datatype	&other)	const
{
htri_t	res	=	H5Tequal(id,	other.get_id());
if	(res	<	0)
throw	Exception("cannot	compare	datatypes");
return	res	!=	0;
}

void	lock()
{
herr_t	err	=	H5Tlock(get_id());
if	(err	<	0)
throw	Exception("error	locking	datatype");
}
};



/*
RW	abstracts	away	how	datasets	and	attributes	are	written	since	both	works
the	same	way,	afik,	except	for	function	names,	e.g.	H5Awrite	vs	H5Dwrite.
*/
class	RW
{
public:
virtual	void	write(const	void*	buf)	=	0;
virtual	void	read(void	*buf)	=	0;
virtual	~RW()	{}
};

class	RWdataset	:	public	RW
{
hid_t	ds_id,	mem_type_id,	mem_space_id,	file_space_id;
public:
RWdataset(hid_t	ds_id_,	hid_t	mem_type_id_,	hid_t	mem_space_id_,	hid_t	file_space_id_)	:	ds_id(ds_id_),	mem_type_id(mem_type_id_),	mem_space_id(mem_space_id_),	file_space_id(file_space_id_)	{}
void	write(const	void*	buf)
{
herr_t	err	=	H5Dwrite(ds_id,	mem_type_id,	mem_space_id,	file_space_id,	H5P_DEFAULT,	buf);
if	(err	<	0)
throw	Exception("error	writing	to	dataset");
}
void	read(void	*buf)
{
herr_t	err	=	H5Dread(ds_id,	mem_type_id,	mem_space_id,	file_space_id,	H5P_DEFAULT,	buf);
if	(err	<	0)
throw	Exception("error	reading	from	dataset");
}
};


class	RWattribute	:	public	RW
{
hid_t	attr_id,	mem_type_id;
public:
RWattribute(hid_t	attr_id_,	hid_t	mem_type_id_)	:	attr_id(attr_id_),	mem_type_id(mem_type_id_)	{}

void	write(const	void*	buf)
{
herr_t	err	=	H5Awrite(attr_id,	mem_type_id,	buf);
if	(err	<	0)
throw	Exception("error	writing	to	attribute");
}
void	read(void	*buf)
{
herr_t	err	=	H5Aread(attr_id,	mem_type_id,	buf);
if	(err	<	0)
throw	Exception("error	reading	from	attribute");
}
};


template<class	T>
struct	h5traits_of;



class	Dataspace	:	public	Object
{
friend	class	Dataset;
friend	class	Attributes;
friend	class	Attribute;
private:
Dataspace(hid_t	id,	internal::NoIncRC)	:	Object(id)	{}
Dataspace(hid_t	id,	internal::IncRC)	:	Object(id,	internal::IncRC())	{}
public:
Dataspace()	:	Object()	{}

virtual	~Dataspace()
{
if	(this->id	==	H5S_ALL)
this->id	=	-1;
}

static	Dataspace	simple(int	rank,	const	hsize_t*	dims)
{
hid_t	id	=	H5Screate_simple(rank,	dims,	NULL);
if	(id	<	0)
{
std::ostringstream	oss;
oss	<<	"error	creating	dataspace	with	rank	"	<<	rank	<<	"	and	sizes	";
for	(int	i	=	0;	i<rank;	++i)
oss	<<	dims[i]	<<	",";
throw	Exception(oss.str());
}
return	Dataspace(id,	internal::NoIncRC());
}

static	Dataspace	scalar()
{
hid_t	id	=	H5Screate(H5S_SCALAR);
if	(id	<	0)
throw	Exception("error	creating	scalar	dataspace");
return	Dataspace(id,	internal::NoIncRC());
}

/*
Arguments	specify	dimensions.	The	rank	is	determined	from	the	first	argument	that	is	zero.
E.g.	simple_dims(5,	6)	and	simple_dims(5,	6	0	10)	both	result	in	a	dataspace
of	rank	two.	Any	arguments	following	a	zero	are	ignored.
*/
static	Dataspace	simple_dims(hsize_t	dim0,	hsize_t	dim1	=	0	hsize_t	dim2	=	0	hsize_t	dim3	=	0	hsize_t	dim4
{
enum	{	MAX_DIM	=	6	};
const	hsize_t	xa[MAX_DIM]	=	{	(hsize_t)dim0,	(hsize_t)dim1,	(hsize_t)dim2,	(hsize_t)dim3,	(hsize_t)dim4,	(hsize_t)dim5	};
int	rank	=	0;
for	(;	rank<MAX_DIM;	++rank)	{	//	find	rank
if	(xa[rank]	<=	0)	break;
}
if	(rank	<=	0)
throw	Exception("nd	dataspace	with	rank	0	not	permitted,	use	create_scalar()");
return	Dataspace::simple(rank,	xa);
}

H5S_sel_type	get_selection_type()	const
{
H5S_sel_type	sel	=	H5Sget_select_type(get_id());
if	(sel	<	0)
throw	Exception("error	geting	select	type");
return	sel;
}

int	get_rank()	const
{
int	r	=	H5Sget_simple_extent_ndims(this->id);
if	(r	<	0)
throw	Exception("unable	to	get	dataspace	rank");
return	r;
}

int	get_dims(hsize_t	*dims)	const
{
int	r	=	H5Sget_simple_extent_dims(this->id,	dims,	NULL);
if	(r	<	0)
throw	Exception("unable	to	get	dataspace	dimensions");
return	r;
}

bool	is_simple()	const
{
htri_t	r	=	H5Sis_simple(this->id);
if	(r	<	0)
throw	Exception("unable	to	determine	if	dataspace	is	simple");
return	r	>	0;
}

hssize_t	get_npoints()	const
{
hssize_t	r	=	H5Sget_simple_extent_npoints(this->id);
if	(r	<=	0)
throw	Exception("unable	to	determine	number	of	elements	in	dataspace");
return	r;
}

void	select_hyperslab(hsize_t*	offset,	hsize_t*	stride,	hsize_t*	count,	hsize_t	*block)
{
herr_t	r=	H5Sselect_hyperslab(get_id(),	H5S_SELECT_SET,	offset,	stride,	count,	block);
if	(r	<	0)
throw	Exception("unable	to	select	hyperslab");
}

void	select_all()
{
herr_t	r	=	H5Sselect_all(get_id());
if	(r	<	0)
throw	Exception("error	selecting	the	entire	extent");
}

hssize_t	get_select_npoints()	const
{
hssize_t	r	=	H5Sget_select_npoints(get_id())	;
if	(r	<	0)
throw	Exception("unable	to	get	number	of	selected	points");
return	r;
}

bool	is_extent_equal(const	Dataspace	&other)
{
htri_t	result	=	H5Sextent_equal(get_id(),	other.get_id());
if	(result	<	0)
throw	Exception("error	determining	if	dataspace	extents	are	equal");
return	bool(result);
}
};


class	Attribute	:	public	Object
{
friend	class	Attributes;
friend	class	RWattribute;
using	Object::inc_ref;
private:
Attribute(hid_t	id,	internal::NoIncRC)	:	Object(id)	{}

Attribute(hid_t	loc_id,	const	std::string	&name,	hid_t	type_id,	hid_t	space_id,	hid_t	acpl_id,	hid_t	aapl_id,	internal::TagCreate)
{
this->id	=	H5Acreate2(loc_id,	name.c_str(),	type_id,	space_id,	acpl_id,	aapl_id);
if	(this->id	<	0)
throw	Exception("error	creating	attribute:	"+name);
}

Attribute(hid_t	obj_id,	const	std::string	&name,	hid_t	aapl_id,	internal::TagOpen)
{
htri_t	e	=	H5Aexists(obj_id,	name.c_str());
if	(e	==	0)
throw	NameLookupError(name);
else	if	(e	<	0)
throw	Exception("error	checking	presence	of	attribute:	"+name);
this->id	=	H5Aopen(obj_id,	name.c_str(),	aapl_id);
if	(this->id	<	0)
throw	Exception("error	opening	attribute:	"+name);
}

public:
Attribute()	{}

Dataspace	get_dataspace()	const
{
hid_t	id	=	H5Aget_space(this->id);
if	(id	<	0)
throw	Exception("unable	to	get	dataspace	of	Attribute");
return	Dataspace(id,	internal::NoIncRC());
}

Datatype	get_datatype()	const
{
hid_t	type_id	=	H5Aget_type(this->id);
if	(type_id<0)
throw	Exception("unable	to	get	type	of	Attribute");
return	Datatype(type_id);
}

/*	assuming	that	the	memory	dataspace	equals	disk	dataspace	*/
template<class	T>
void	read(T	*values)	const
{
Datatype	memtype	=	get_memtype<T>();
RWattribute	rw(this->get_id(),	memtype.get_id());
h5traits_of<T>::type::read(rw,	memtype,	get_dataspace(),	values);
}

/*	assuming	that	the	memory	dataspace	equals	disk	dataspace	*/
template<class	T>
void	write(T*	values)
{
Datatype	memtype	=	get_memtype<T>();
RWattribute	rw(this->get_id(),	memtype.get_id());
h5traits_of<T>::type::write(rw,	memtype,	get_dataspace(),	values);
}
};




class	Attributes
{
private:
Object	attributed_object;

public:
Attributes()	:	attributed_object()	{}
Attributes(const	Object	&o)	:	attributed_object(o)	{}

Attribute	open(const	std::string	&name)
{
return	Attribute(attributed_object.get_id(),	name,	H5P_DEFAULT,	internal::TagOpen());
}

template<class	T>
Attribute	create(const	std::string	&name,	const	Dataspace	&space)
{
Datatype	disktype	=	get_disktype<T>();
Attribute	a(attributed_object.get_id(),
name,
disktype.get_id(),
space.get_id(),
H5P_DEFAULT,	H5P_DEFAULT,
internal::TagCreate());
return	a;
}

template<class	T>
void	create(const	std::string	&name,	const	T	&value)
{
auto	sp	=	Dataspace::scalar();
create<T>(name,	sp).write(&value);
}

template<class	T>
void	create(const	std::string	&name,	Dataspace	space,	const	T	*values)
{
create<T>(name,	space).write(values);
}

template<class	T>
void	set(const	std::string	&name,	Dataspace	space,	const	T*	values)
{
Attribute	a;
if	(exists(name))
{
a	=	open(name);
if	(a.get_dataspace().is_extent_equal(space))
{
AutoErrorReportingGuard	disableErrorOutput;
try
{

a.write(values);
return;
}
catch	(const	Exception	&)
{
remove(name);
}
}
else
remove(name);
}
a	=	create<T>(name,	space);
a.write(values);
return;
}

template<class	T>
void	set(const	std::string	&name,	const	T	&value)
{
set(name,	Dataspace::scalar(),	&value);
}

bool	exists(const	std::string	&name)	const
{
htri_t	res	=	H5Aexists_by_name(attributed_object.get_id(),	".",	name.c_str(),	H5P_DEFAULT);
if	(res>0)	return	true;
else	if	(res==0)	return	false;
else	throw	Exception("error	looking	for	attribute	by	name");
}

hsize_t	size()	const
{
hid_t	objid	=	attributed_object.get_id();
H5O_info_t	info;
herr_t	err	=	H5Oget_info(objid,	&info);
if	(err	<	0)
throw	Exception("error	getting	the	number	of	attributes	on	an	object");
return	info.num_attrs;
}

template<class	T>
void	get(const	std::string	&name,	T	&value)
{
open(name).read(&value);
}

template<class	T>
T	get(const	std::string	&name)
{
T	ret;
get(name,	ret);
return	ret;
}

#ifdef	HDF_WRAPPER_HAS_BOOST
template<class	T>
boost::optional<T>	try_get(const	std::string	&name)
{
if	(exists(name))	return	boost::optional<T>(get<T>(name));
else	return	boost::optional<T>();
}
#endif

void	remove(const	std::string	&name)
{
herr_t	err	=	H5Adelete(attributed_object.get_id(),	name.c_str());
if	(err	<	0)
throw	Exception("error	deleting	attribute");
}
};


class	Properties	:	protected	Object
{
public:
using	Object::get_id;
using	Object::is_valid;
using	Object::is_same;

Properties(hid_t	cls_id)	:	Object()
{
this->id	=	H5Pcreate(cls_id);
if	(this->id	<	0)
throw	Exception("error	creating	property	list");
}

Properties&	deflate(int	strength	=	9)
{
H5Pset_deflate(this->id,	strength);
return	*this;
};

Properties&	chunked(int	rank,	const	hsize_t	*dims)
{
H5Pset_chunk(this->id,	rank,	dims);
return	*this;
}

Properties&	chunked_with_estimated_size(const	Dataspace	&sp)
{
hsize_t	dims[H5S_MAX_RANK];
int	r	=	sp.get_dims(dims);
hsize_t	cdims[H5S_MAX_RANK];
for	(int	i=0;	i<r;	++i)
{
hsize_t	val	=	dims[i];
hsize_t	org_val	=	val;
val	=	(hsize_t)(val	*	0.1);
if	(val	<	32.)
val	=	32;
if	(val	>	org_val)
val	=	org_val;
cdims[i]	=	val;
}
return	chunked(r,	cdims);
}
};


class	iterator;

class	Group	:	public	Object
{
friend	class	File;
private:
Group(hid_t	loc_id,	const	char	*	name,	hid_t	gapl_id	,	internal::TagOpen)
{
this->id	=	H5Gopen2(loc_id,	name,	gapl_id);
if	(this->id	<	0)
throw	Exception("unable	to	open	group:	"+std::string(name));
}
Group(hid_t	loc_id,	const	char	*name,	hid_t	lcpl_id,	hid_t	gcpl_id,	hid_t	gapl_id,	internal::TagCreate)
{
this->id	=	H5Gcreate2(loc_id,	name,	lcpl_id,	gcpl_id,	gapl_id);
if	(this->id	<	0)
throw	Exception("unable	to	create	group:	"+std::string(name));
}
public:

Group()	:	Object()	{}
explicit	Group(hid_t	id)	:	Object(id)	{	Object::inc_ref();	}

bool	exists(const	std::string	&name)	const
{
htri_t	res	=	H5Lexists(this->id,	name.c_str(),	H5P_DEFAULT);
if	(res	<	0)
throw	Exception("cannot	determine	existence	of	link");
return	res	>	0;
}

//	number	of	links	in	the	group
hsize_t	size()	const
{
H5G_info_t	info;
herr_t	res	=	H5Gget_info(this->id,	&info);
if	(res	<	0)
throw	Exception("cannot	get	info	of	group");
return	info.nlinks;
}

//	used	to	iterate	over	links	in	the	group,	up	to	size()
std::string	get_link_name(hsize_t	idx)	const
{
char	buffer[4096];
ssize_t	n	=	H5Lget_name_by_idx(this->id,	".",	H5_INDEX_NAME,	H5_ITER_NATIVE,	(hsize_t)idx,	buffer,	4096	H5P_DEFAULT);
if	(n	<	0)
throw	Exception("cannot	get	name	of	link	in	group");
return	std::string(buffer,	n);
}

Group	create_group(const	std::string	&name)
{
return	Group(this->id,	name.c_str(),	H5P_DEFAULT,	H5P_DEFAULT,	H5P_DEFAULT,	internal::TagCreate());
}

Group	open_group(const	std::string	&name)
{
return	Group(this->id,	name.c_str(),	H5P_DEFAULT,	internal::TagOpen());
}

Group	require_group(const	std::string	&name,	bool	*had_group	=	NULL)
{
if	(exists(name))
{
if	(had_group)	*had_group	=	true;
return	open_group(name);
}
else
{
if	(had_group)	*had_group	=	false;
return	create_group(name);
}
}

Attributes	attrs()
{
return	Attributes(*this);
}

Dataset	open_dataset(const	std::string	&name);

#ifdef	HDF_WRAPPER_HAS_BOOST
boost::optional<Dataset>	try_open_dataset(const	std::string	&name);
#endif

void	remove(const	std::string	&name)
{
herr_t	err	=	H5Ldelete(get_id(),	name.c_str(),	H5P_DEFAULT);
if	(err	<	0)
throw	Exception("cannot	remove	link	from	group");
}

iterator	begin();
iterator	end();
};


/*
TODO:	i	probably	want	an	iterator	that	returns	Object	references.	Objects	because
items	in	groups	can	be	datasets	or	other	groups.	For	this	to	be	usefull,
i	need	a	mechanism	to	cast	Object	to	derived	classes.
*/
class	iterator	:	public	std::iterator<std::bidirectional_iterator_tag,	std::string>
{
hssize_t	idx;
Group	g;
friend	class	Group;
iterator(hssize_t	idx_,	Group	g_)	:	idx(idx_),	g(g_)	{}
public:
iterator()	:	idx(std::numeric_limits<int>::max())	{}
iterator&	operator++()	{	++idx;	return	*this;	}
iterator&	operator--()	{	--idx;	return	*this;	}
iterator	operator++(int)	{	auto	tmp	=	*this;	++idx;	return	tmp;	}
iterator	operator--(int)	{	auto	tmp	=	*this;	--idx;	return	tmp;	}
bool	operator==(const	iterator	&other)	const	{	return	other.idx	==	idx;	assert(other.g.is_same(g));	}
bool	operator!=(const	iterator	&other)	const	{	return	!(*this	==	other);	}
std::string	dereference()	const
{
return	g.get_link_name(idx);
}
std::string	operator*()	const	{	return	dereference();	}
};


inline	iterator	Group::begin()
{
return	iterator(0,	*this);
}

inline	iterator	Group::end()
{
return	iterator(this->size(),	*this);
}


class	File	:	public	Object
{
File(hid_t	id,	internal::NoIncRC)	:	Object(id)	{}	//	takes	a	file	handle	that	needs	to	be	closed.
friend	class	Object;	//	because	Object	need	to	construct	File	using	the	above	constructor.
public:
explicit	File(hid_t	id)	:	Object()	{	this->inc_ref();	}	//	a	logical	copy	of	the	original	given	by	id,

/*
w	=	create	or	truncate	existing	file
a	=	append	to	file	or	create	new	file
r	=	read	only;	file	must	exist
w-	=	new	file;	file	must	not	already	exist
r+	=	read/write;	file	must	exist
*/
File(const	std::string	&name,	const	std::string	openmode	=	"w")	:	Object()
{
bool	call_open	=	true;
unsigned	int	flags;
if	(openmode	==	"w")
{
flags	=	H5F_ACC_TRUNC;
call_open	=	false;
}
else	if	(openmode	==	"a")	//	append	at	existing	file	or	create	new	file
{
if	(H5Fis_hdf5(name.c_str())>0)	//	file	exists,	so	open
{
call_open	=	true;
flags	=	H5F_ACC_RDWR;
}
else	//	create	file
{
call_open	=	false;
flags	=	H5F_ACC_TRUNC;
}
}
else	if	(openmode	==	"w-")
{
flags	=	H5F_ACC_EXCL;
call_open	=	false;
}
else	if	(openmode	==	"r")
flags	=	H5F_ACC_RDONLY;
else	if	(openmode	==	"r+")
flags	=	H5F_ACC_RDWR;
else
throw	Exception("bad	openmode:	"	+	openmode);
if	(call_open)
this->id	=	H5Fopen(name.c_str(),	flags	,	H5P_DEFAULT);
else
this->id	=	H5Fcreate(name.c_str(),	flags	,	H5P_DEFAULT,	H5P_DEFAULT);
if	(this->id	<	0)
throw	Exception("unable	to	open	file:	"	+	name);
}

File()	:	Object()	{}

void	open(const	std::string	&name,	const	std::string	openmode	=	"w")
{
this->~File();
new	(this)	File(name,	openmode);
}

void	close()
{
if	(this->id	==	-1)	return;
herr_t	err	=	H5Fclose(this->id);
this->id	=	-1;
if	(err	<	0)
throw	Exception("unable	to	close	file");
}

Group	root()
{
return	Group(this->id,	"/",	H5P_DEFAULT,	internal::TagOpen());
}

void	flush()
{
herr_t	err	=	H5Fflush(this->id,	H5F_SCOPE_LOCAL);
if	(err	<	0)
throw	Exception("unable	to	flush	file");
}
};



inline	File	Object::get_file()	const
{
hid_t	fid	=	H5Iget_file_id(id);	//	it	seems	to	increase	the	reference	count	by	its	own.	(The	returned	id
//	https://www.hdfgroup.org/HDF5/doc/RM/RM_H5I.html#Identify-GetFileId
if	(fid	<	0)
throw	Exception("cannot	get	file	id	from	object");
return	File(fid,	internal::NoIncRC());
}


enum	DsCreationFlags
{
CREATE_DS_0	=	0
CREATE_DS_COMPRESSED	=	1
CREATE_DS_CHUNKED	=	2
#ifndef	HDF_WRAPPER_DS_CREATION_DEFAULT_FLAGS
#ifdef	H5_HAVE_FILTER_DEFLATE
CREATE_DS_DEFAULT	=	CREATE_DS_COMPRESSED
#else
CREATE_DS_DEFAULT	=	CREATE_DS_0
#endif
#else
CREATE_DS_DEFAULT	=	HDF_WRAPPER_DS_CREATION_DEFAULT_FLAGS
#endif
};





class	Dataset	:	public	Object
{
friend	class	Group;
private:
Dataset(hid_t	loc_id,	const	std::string	&name,	hid_t	dapl_id,	internal::TagOpen)
{
this->id	=	H5Dopen2(loc_id,	name.c_str(),	dapl_id);
if	(this->id	<	0)
throw	Exception("unable	to	open	dataset:	"+name);
}

template<class	T>
void	write(Dataspace	memspace,	hid_t	disk_space_id,	const	T*	data)
{
Datatype	memtype	=	get_memtype<T>();
RWdataset	rw(get_id(),	memtype.get_id(),	memspace.get_id(),	disk_space_id);
h5traits_of<T>::type::write(rw,	memtype,	memspace,	data);
}

template<class	T>
void	read(Dataspace	memspace,	hid_t	disk_space_id,	T*	data)	const
{
Datatype	memtype	=	get_memtype<T>();
RWdataset	rw(get_id(),	memtype.get_id(),	memspace.get_id(),	disk_space_id);
h5traits_of<T>::type::read(rw,	memtype,	memspace,	data);
}

Dataset(hid_t	id,	internal::NoIncRC)	:	Object(id)	{}	//	we	get	an	existing	reference,	no	need	to	increase	the	ref

public:
Dataset()	:	Object()	{}
explicit	Dataset(hid_t	id)	:	Object(id)	{	Object::inc_ref();	}	//	we	manage	the	new	reference

static	Dataset	create(Group	group,	const	std::string	&name,	const	Datatype&	dtype,	const	Dataspace	&space,	const	Properties	&prop)
{
hid_t	id	=	H5Dcreate2(group.get_id(),	name.c_str(),
dtype.get_id(),	space.get_id(),
H5P_DEFAULT,	prop.get_id(),	H5P_DEFAULT);
if	(id	<	0)
throw	Exception("error	creating	dataset:	"+name);
return	Dataset(id,	internal::NoIncRC());
}

template<class	T>
static	Dataset	create(Group	group,	const	std::string	&name,	const	Dataspace	&space,	DsCreationFlags	flags	=	CREATE_DS_DEFAULT)
{
return	Dataset::create(group,	name,	get_disktype<T>(),	space,	create_creation_properties(space,	flags));
}

template<class	T>
void	write(const	Dataspace	&mem_space,	const	Dataspace	&file_space,	const	T*	data)
{
write(mem_space,	file_space.get_id(),	data);
}

template<class	T>
void	write(const	T*	data)
{
Dataspace	ds	=	get_dataspace();
write(ds,	H5S_ALL,	data);
}

static	Properties	create_creation_properties(const	Dataspace	&sp,	DsCreationFlags	flags)
{
Properties	prop(H5P_DATASET_CREATE);
if	(flags	&	CREATE_DS_COMPRESSED)
prop.deflate();
if	(flags	&	CREATE_DS_CHUNKED	||	flags	&	CREATE_DS_COMPRESSED)
prop.chunked_with_estimated_size(sp);
return	prop;
}

Attributes	attrs()
{
return	Attributes(*this);
}

Dataspace	get_dataspace()	const
{
hid_t	id	=	H5Dget_space(this->id);
if	(id	<	0)
throw	Exception("unable	to	get	dataspace	of	dataset");
return	Dataspace(id,	internal::NoIncRC());
}

Datatype	get_datatype()	const
{
hid_t	type_id	=	H5Dget_type(this->id);
if	(type_id<0)
throw	Exception("unable	to	get	type	of	Attribute");
return	Datatype(type_id);
}

template<class	T>
void	read(T	*data)	const
{
Dataspace	ds	=	get_dataspace();
read(ds,	H5S_ALL,	data);
}
};





inline	Dataset	Group::open_dataset(const	std::string	&name)
{
return	Dataset(this->id,	name,	H5P_DEFAULT,	internal::TagOpen());
}

#ifdef	HDF_WRAPPER_HAS_BOOST
inline	boost::optional<Dataset>	Group::try_open_dataset(const	std::string	&name)
{
hid_t	id;
{
AutoErrorReportingGuard	guard;
guard.disableReporting();
id	=	H5Dopen2(this->id,	name.c_str(),	H5P_DEFAULT);
}
if	(id	<	0)
{
if	(exists(name))	//	well	so	the	dataset	exists	but	for	some	reason	it	cannot	be	opened	->	error
throw	Exception("unable	to	open	existing	item	as	dataset:	"+name);
else
return	boost::optional<Dataset>();	//	no	dataset	under	this	name
}
else	return	boost::optional<Dataset>(Dataset(id,	internal::NoIncRC()));
}
#endif




namespace	internal
{

/*	This	section	deals	with	creation	of	Datatype	objects	for	basic	data	types.
Copy	is	used	on	HDF	API	constants	like	"H5T_NATIVE_INT"	because	Datatype
object	cannot	handle	these	constants.	This	is	because	H5T_NATIVE_INT,	etc.
are	apparently	not	real	objects,	i.e.	H5O_isvalid	indicates	false.	Some	special
case	handling	for	these	constants	might	be	needed	instead.
*/
template<class	T>
inline	Datatype	get_memtype()
{
//	writing	sizeof(T)==0	seems	to	work	for	gcc.	just	writing	false	always	causes	an	error????!
static_assert	(sizeof(T)==0,	"specialize	me!");
return	Datatype();
}

template<class	T>
inline	Datatype	get_disktype()
{
static_assert	(sizeof(T)==0,	"specialize	me!");
return	Datatype();
}


#define	HDF5_WRAPPER_SPECIALIZE_TYPE(T,	tid,	dtid)	\
template<>	inline	Datatype	get_memtype<T>()	\
{	\
return	Datatype::copy(tid);	\
}	\
template<>	inline	Datatype	get_disktype<T>()	\
{	\
return	Datatype::copy(dtid);	\
}


HDF5_WRAPPER_SPECIALIZE_TYPE(int,	H5T_NATIVE_INT,	H5T_STD_I32LE)
HDF5_WRAPPER_SPECIALIZE_TYPE(unsigned	int,	H5T_NATIVE_UINT,	H5T_STD_U32LE)
HDF5_WRAPPER_SPECIALIZE_TYPE(unsigned	long	long,	H5T_NATIVE_ULLONG,	H5T_STD_U64LE)
HDF5_WRAPPER_SPECIALIZE_TYPE(long	long,	H5T_NATIVE_LLONG,	H5T_STD_I64LE)
HDF5_WRAPPER_SPECIALIZE_TYPE(char,	H5T_NATIVE_CHAR,	H5T_STD_I8LE)
HDF5_WRAPPER_SPECIALIZE_TYPE(unsigned	char,	H5T_NATIVE_UCHAR,	H5T_STD_U8LE)
HDF5_WRAPPER_SPECIALIZE_TYPE(float,	H5T_NATIVE_FLOAT,	H5T_IEEE_F32LE)
HDF5_WRAPPER_SPECIALIZE_TYPE(double,	H5T_NATIVE_DOUBLE,	H5T_IEEE_F64LE)
HDF5_WRAPPER_SPECIALIZE_TYPE(bool,	H5T_NATIVE_CHAR,	H5T_STD_U8LE)
HDF5_WRAPPER_SPECIALIZE_TYPE(unsigned	long,	H5T_NATIVE_ULONG,	H5T_STD_U64LE)
HDF5_WRAPPER_SPECIALIZE_TYPE(long,	H5T_NATIVE_LONG,	H5T_STD_I64LE)


template<>	inline	Datatype	get_memtype<const	char	*>()
{
Datatype	dt	=	Datatype::copy(H5T_C_S1);
dt.set_variable_size();
return	dt;
}
template<>	inline	Datatype	get_disktype<const	char	*>()
{
return	get_memtype<const	char*>();
}

template<>	inline	Datatype	get_memtype<char*>()
{
return	get_memtype<const	char*>();
}

template<>	inline	Datatype	get_disktype<char	*>()
{
return	get_memtype<char*>();
}

}	//	namespace	internal




template<class	T>
struct	h5traits
{
static	inline	Datatype	get_memtype()
{
return	internal::get_memtype<T>();
}

static	inline	Datatype	get_disktype()
{
return	internal::get_disktype<T>();
}

static	inline	void	write(RW	&rw,	const	Datatype	&memtype,	const	Dataspace	&memspace,	const	T	*values)
{
rw.write(values);
}

static	inline	void	read(RW	&rw,	const	Datatype	&memtype,	const	Dataspace	&memspace,	T	*values)
{
rw.read(values);
}
};


template<>
struct	h5traits<std::string>
{
static	inline	Datatype	get_memtype()
{
return	internal::get_memtype<char*>();
}

static	inline	Datatype	get_disktype()
{
return	internal::get_disktype<char*>();
}

static	inline	void	write(RW	&rw,	const	Datatype	&memtype,	const	Dataspace	&memspace,	const	std::string	*values)
{
hssize_t	n	=	memspace.get_npoints();
assert(n	>=	1);
std::vector<const	char*>	s(n);
for	(int	i=0;	i<n;	++i)	s[i]	=	values[i].c_str();
rw.write(&s[0]);
}

static	inline	void	read(RW	&rw,	const	Datatype	&memtype,	const	Dataspace	&memspace,	std::string	*values)
{
hssize_t	n	=	memspace.get_npoints();
assert(n	>=	1);
assert(H5Tis_variable_str(memtype.get_id()));

//	see	https://www.hdfgroup.org/HDF5/doc1.6/UG/11_Datatypes.html
//	variable	data	sets.	string	types	(char*,	char[n],	std::string)	are	mapped	by	default	to	variable	length	data	types

std::vector<char*>	buffers(n);
rw.read(&buffers[0]);
for	(hssize_t	i	=	0;	i	<	n;	++i)
{
values[i].assign(buffers[i]);
}
//	release	the	stuff	that	hdf5	allocated
H5Dvlen_reclaim(memtype.get_id(),	memspace.get_id(),	H5P_DEFAULT,	&buffers[0]);
}
};


template<size_t	n>
struct	h5traits<char[n]>
{
typedef	char	CharArray[n];

static	inline	Datatype	get_memtype()
{
return	internal::get_memtype<char*>();
}

static	inline	Datatype	get_disktype()
{
return	internal::get_disktype<char*>();
}

static	inline	void	write(RW	&rw,	const	Datatype	&memtype,	const	Dataspace	&memspace,	const	CharArray	*values)
{
std::vector<const	char*>	s(memspace.get_npoints());
for	(int	i=0;	i<s.size();	++i)	s[i]	=	values[i];	//	because	i	don't	know	how	to	deal	with	an
rw.write(&s[0]);
}
};


template<>
struct	h5traits	<	char*	>
{
static	inline	Datatype	get_memtype()
{
return	internal::get_memtype<char*>();
}

static	inline	Datatype	get_disktype()
{
return	internal::get_disktype<char*>();
}

static	inline	void	write(RW	&rw,	const	Datatype	&memtype,	const	Dataspace	&memspace,	const	char	*const	*values)
{
rw.write(values);
}
//	TODO:	enable	reading.	Needs	treatment	similar	to	std::string.	Don't	want	references	to	memory	allocated	by	HDF5	lib
};


//	the	rest	of	the	api	uses	h5traits_of	to	get	rid	of	const	and	volatile	qualifiers
template<class	T>
struct	h5traits_of
{
typedef	typename	std::remove_cv<T>::type	stripped_type;
typedef	h5traits<stripped_type>	type;	//	the	resulting	"clean"	traits	type
};


/*
Here	is	this	super	ugly	code	which	caches	the	result	of	the	construction	of	HDF5
types	in	static,	i.e.	global	variables.	The	mechanism	uses	hid_t	as	static
variable	type,	because	using	Datatype	would	result	in	destructor	calls,
and	concomitant	api	calls	when	the	hdf	lib	is	unloaded	already.
*/
namespace	internal
{

inline	hid_t	prep_type_cache(Datatype	dt)
{
dt.lock();
dt.inc_ref();
return	dt.get_id();
}

}

//	wrap	complicated	things	in	neat	api	functions
template<class	T>
inline	Datatype	get_disktype()
{
static	hid_t	id	=	internal::prep_type_cache(h5traits_of<T>::type::get_disktype());
return	Datatype(id,	internal::IncRC());
}

template<class	T>
inline	Datatype	get_memtype()
{
static	hid_t	id	=	internal::prep_type_cache(h5traits_of<T>::type::get_memtype());
return	Datatype(id,	internal::IncRC());
}

}	//	namespace	h5cpp



namespace	h5cpp
{

/*==================================================
*	some	free	functions
*===================================================*/

#if	0
template<class	iterator>
inline	Dataspace	create_dataspace_from_iter(iterator	begin,	iterator	end)
{
hsize_t	dims[H5S_MAX_RANK];
int	i	=	0;
for	(;	begin	!=	end;	++begin)
{
dims[i++]	=	*begin;

if	(i	>=	H5S_MAX_RANK)
throw	Exception("error	creating	dataspace:	provided	range	is	too	large");
}
return	Dataspace::simple(i,	dims);
}

template<class	T,	class	A>
inline	Dataspace	create_dataspace_simple(const	std::vector<T,	A>	&dims)
{
assert(dims.size()	>	0);
return	Dataspace::simple(dims.size(),	&dims[0]);
}


template<class	T,	class	A>
inline	void	get_dims(const	Dataspace	&sp,	std::vector<T,	A>	&ret)
{
hsize_t	dims[H5S_MAX_RANK];
int	r	=	sp.get_dims(dims);
ret.resize(r);
std::copy(dims,	dims	+	r,	ret.begin());
}
#endif
/*--------------------------------------------------
*	datasets
*	------------------------------------------------	*/



template<class	T>
inline	Dataset	create_dataset(Group	group,	const	std::string	&name,	const	Dataspace	&sp,	const	T*	data	=	nullptr,	DsCreationFlags	flags	=
{
Dataset	ds	=	Dataset::create(group,	name,	get_disktype<T>(),	sp,	Dataset::create_creation_properties(sp,	flags));
if	(data	!=	nullptr)
ds.write<T>(data);
return	ds;
}

template<class	T>
inline	Dataset	create_dataset_scalar(Group	group,	const	std::string	&name,	const	T&	data)
{
Dataspace	sp	=	Dataspace::scalar();
Dataset	ds	=	Dataset::create(group,	name,	get_disktype<T>(),	sp,	Dataset::create_creation_properties(sp,	CREATE_DS_0));
ds.write<T>(&data);
return	ds;
}


template<class	T,	class	A>
inline	Dataset	create_dataset(Group	group,	const	std::string	&name,	const	std::vector<T,	A>	&data,	DsCreationFlags	flags	=	CREATE_DS_DEFAULT)
{
return	create_dataset(group,	name,	Dataspace::simple_dims(data.size()),	&data[0],	flags);
}


template<class	T,	class	A>
inline	void	read_dataset(const	Dataset	ds,	std::vector<T,	A>	&ret)
{
Dataspace	sp	=	ds.get_dataspace();
ret.resize(sp.get_npoints());
ds.read(&ret[0]);
}


/*--------------------------------------------------
*	Attributes
*	------------------------------------------------	*/

template<class	T,	class	A>
inline	void	set_array(Attributes	attrs,	const	std::string	&name,	const	std::vector<T,	A>	&data)
{
attrs.set(name,	Dataspace::simple_dims(data.size()),	&data[0]);
}

template<class	T>
inline	void	set_array(Attributes	attrs,	const	std::string	&name,	const	T*	data,	hsize_t	count)
{
attrs.set(name,	Dataspace::simple_dims(count),	data);
}

template<class	T>
inline	void	set(Attributes	attrs,	const	std::string	&name,	const	T	&value)
{
attrs.set(name,	Dataspace::scalar(),	&value);
}

template<class	T,	class	A>
inline	void	get_array(Attributes	attrs,	const	std::string	&name,	std::vector<T,	A>	&ret)
{
Attribute	a	=	attrs.open(name);
ret.resize(a.get_dataspace().get_npoints());
a.read<T>(&ret[0]);
}

template<class	T>
inline	void	get(Attributes	attrs,	const	std::string	&name,	T	&value)
{
Attribute	a	=	attrs.open(name);
assert(a.get_dataspace().get_npoints()	==	1);
a.read<T>(&value);
}

}

#endif																	
