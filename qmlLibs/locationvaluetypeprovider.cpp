/****************************************************************************
 **
 ** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtPositioning module of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:LGPL$
 ** Commercial License Usage
 ** Licensees holding valid commercial Qt licenses may use this file in
 ** accordance with the commercial license agreement provided with the
 ** Software or, alternatively, in accordance with the terms contained in
 ** a written agreement between you and Digia.  For licensing terms and
 ** conditions see http://qt.digia.com/licensing.  For further information
 ** use the contact form at http://qt.digia.com/contact-us.
 **
 ** GNU Lesser General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU Lesser
 ** General Public License version 2.1 as published by the Free Software
 ** Foundation and appearing in the file LICENSE.LGPL included in the
 ** packaging of this file.  Please review the following information to
 ** ensure the GNU Lesser General Public License version 2.1 requirements
 ** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Digia gives you certain additional
 ** rights.  These rights are described in the Digia Qt LGPL Exception
 ** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
 **
 ** GNU General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU
 ** General Public License version 3.0 as published by the Free Software
 ** Foundation and appearing in the file LICENSE.GPL included in the
 ** packaging of this file.  Please review the following information to
 ** ensure the GNU General Public License version 3.0 requirements will be
 ** met: http://www.gnu.org/copyleft/gpl.html.
 **
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/
 
#include "locationvaluetypeprovider.h"
#include "qdeclarativecoordinate_p.h"
 
LocationValueTypeProvider::LocationValueTypeProvider()
  :   QQmlValueTypeProvider()
{
}
 
bool LocationValueTypeProvider::create(int type, QQmlValueType *&v)
{
  if (type == qMetaTypeId<QGeoCoordinate>())
    return typedCreate<CoordinateValueType>(v);
 
  return false;
}
 
bool LocationValueTypeProvider::init(int type, void *data, size_t dataSize)
{
  if (type == qMetaTypeId<QGeoCoordinate>())
    return typedInit<QGeoCoordinate>(data, dataSize);
 
  return false;
}
 
bool LocationValueTypeProvider::destroy(int type, void *data, size_t dataSize)
{
  if (type == qMetaTypeId<QGeoCoordinate>())
    return typedDestroy<QGeoCoordinate>(data, dataSize);
 
  return false;
}
 
bool LocationValueTypeProvider::copy(int type, const void *src, void *dst, size_t dstSize)
{
  if (type == qMetaTypeId<QGeoCoordinate>())
    return typedCopyConstruct<QGeoCoordinate>(src, dst, dstSize);
 
  return false;
}
 
bool LocationValueTypeProvider::create(int type, int argc, const void *argv[], QVariant *v)
{
  if (type == qMetaTypeId<QGeoCoordinate>()) {
    if (argc == 2) {
      const float *a = reinterpret_cast<const float *>(argv[0]);
      const float *b = reinterpret_cast<const float *>(argv[1]);
      *v = QVariant::fromValue(QGeoCoordinate(*a, *b));
      return true;
    } else if (argc == 3) {
      const float *a = reinterpret_cast<const float *>(argv[0]);
      const float *b = reinterpret_cast<const float *>(argv[1]);
      const float *c = reinterpret_cast<const float *>(argv[2]);
      *v = QVariant::fromValue(QGeoCoordinate(*a, *b, *c));
      return true;
    }
  }
 
  return false;
}
 
bool LocationValueTypeProvider::createFromString(int type, const QString &s, void *data, size_t dataSize)
{
  Q_UNUSED(data)
    Q_UNUSED(dataSize)
 
    if (type == qMetaTypeId<QGeoCoordinate>()) {
      qWarning("Cannot create value type %d from string '%s'", type, qPrintable(s));
    }
 
  return false;
}
 
bool LocationValueTypeProvider::createStringFrom(int type, const void *data, QString *s)
{
  Q_UNUSED(data)
    Q_UNUSED(s)
 
    if (type == qMetaTypeId<QGeoCoordinate>()) {
      qWarning("Cannot create string from value type %d", type);
    }
 
  return false;
}
 
bool LocationValueTypeProvider::variantFromJsObject(int type, QQmlV4Handle h, QV8Engine *e, QVariant *v)
{
  Q_UNUSED(h)
    Q_UNUSED(e)
    Q_UNUSED(v)
 
    if (type == qMetaTypeId<QGeoCoordinate>()) {
      qWarning("Cannot create variant from js object for type %d", type);
    }
 
  return false;
}
 
bool LocationValueTypeProvider::equal(int type, const void *lhs, const void *rhs, size_t rhsSize)
{
  Q_UNUSED(rhsSize)
 
    if (type == qMetaTypeId<QGeoCoordinate>())
      return typedEqual<QGeoCoordinate>(lhs, rhs);
 
  return false;
}
 
bool LocationValueTypeProvider::store(int type, const void *src, void *dst, size_t dstSize)
{
  if (type == qMetaTypeId<QGeoCoordinate>())
    return typedStore<QGeoCoordinate>(src, dst, dstSize);
 
  return false;
}
 
bool LocationValueTypeProvider::read(int srcType, const void *src, size_t srcSize, int dstType, void *dst)
{
  if (srcType == qMetaTypeId<QGeoCoordinate>())
    return typedRead<QGeoCoordinate>(srcType, src, srcSize, dstType, dst);
 
  return false;
}
 
bool LocationValueTypeProvider::write(int type, const void *src, void *dst, size_t dstSize)
{
  if (type == qMetaTypeId<QGeoCoordinate>())
    return typedWrite<QGeoCoordinate>(src, dst, dstSize);
 
  return false;
}
