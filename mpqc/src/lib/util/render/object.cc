//
// object.cc
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Curtis Janssen <cljanss@limitpt.com>
// Maintainer: LPS
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdlib.h>
#include <util/misc/formio.h>
#include <util/render/render.h>
#include <util/render/object.h>

#define CLASSNAME RenderedObject
#define PARENTS public DescribedClass
#include <util/class/classia.h>
void *
RenderedObject::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] = DescribedClass::_castdown(cd);
  return do_castdowns(casts,cd);
}

RenderedObject::RenderedObject(const RefMaterial& material):
  material_(material),
  name_(0)
{
}

RenderedObject::RenderedObject(const RefKeyVal& keyval):
  material_(keyval->describedclassvalue("material")),
  transform_(keyval->describedclassvalue("transform")),
  appearance_(keyval->describedclassvalue("appearance")),
  name_(keyval->pcharvalue("name"))
{
}

RenderedObject::~RenderedObject()
{
  
  if (name_) delete[] name_;
}

void
RenderedObject::set_name(const char *name)
{
  delete[] name_;
  if (name) name_ = strcpy(new char[strlen(name)+1],name);
  else name_ = 0;
}

void
RenderedObject::print(ostream& os)
{
  os << "RenderedObject:" << endl;
  if (material_.nonnull()) {
      os << scprintf("  material = 0x%x\n", material_.pointer());
    }
  if (appearance_.nonnull()) {
      os << scprintf("  appearance = 0x%x\n", appearance_.pointer());
    }
  if (transform_.nonnull()) {
      os << scprintf("  transform = 0x%x\n", transform_.pointer());
    }
  os.flush();
}
  

#define CLASSNAME RenderedObjectSet
#define HAVE_KEYVAL_CTOR
#define PARENTS public RenderedObject
#include <util/class/classi.h>
void *
RenderedObjectSet::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] = RenderedObject::_castdown(cd);
  return do_castdowns(casts,cd);
}

RenderedObjectSet::RenderedObjectSet(int capacity)
{
  capacity_ = capacity;
  n_ = 0;
  array_ = new RefRenderedObject[capacity_];
}

RenderedObjectSet::RenderedObjectSet(const RefKeyVal& keyval):
  RenderedObject(keyval)
{
  capacity_ = keyval->count("objects");
  if (keyval->error() != KeyVal::OK) {
      cerr << "RenderedObjectSet: error counting objects" << endl;
      abort();
    }
  n_ = capacity_;
  array_ = new RefRenderedObject[capacity_];
  for (int i=0; i<n_; i++) {
      array_[i] = keyval->describedclassvalue("objects",i);
      if (keyval->error() != KeyVal::OK) {
          cerr << "RenderedObjectSet: error reading objects" << endl;
          abort();
        }
    }
}

RenderedObjectSet::~RenderedObjectSet()
{
  delete[] array_;
}

void
RenderedObjectSet::add(const RefRenderedObject& object)
{
  if (capacity_ == n_) {
      capacity_ += 10;
      RefRenderedObject *tmp = new RefRenderedObject[capacity_];
      for (int i=0; i<n_; i++) {
          tmp[i] = array_[i];
        }
      delete[] array_;
      array_ = tmp;
    }
  array_[n_] = object;
  n_++;
}

void
RenderedObjectSet::render(const RefRender& render)
{
  render->set(this);
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
