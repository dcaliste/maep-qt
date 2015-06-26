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
 
#include "qdeclarativecoordinate_p.h"
 
CoordinateValueType::CoordinateValueType(QObject *parent)
  :   QQmlValueTypeBase<QGeoCoordinate>(qMetaTypeId<QGeoCoordinate>(), parent)
{
}
 
CoordinateValueType::~CoordinateValueType()
{
}
 
/*
  This property holds the value of altitude (meters above sea level).
  If the property has not been set, its default value is NaN.
 
*/
void CoordinateValueType::setAltitude(double altitude)
{
  v.setAltitude(altitude);
}
 
double CoordinateValueType::altitude() const
{
  return v.altitude();
}
 
/*
  This property holds the longitude value of the geographical position
  (decimal degrees). A positive longitude indicates the Eastern Hemisphere,
  and a negative longitude indicates the Western Hemisphere
  If the property has not been set, its default value is NaN.
*/
void CoordinateValueType::setLongitude(double longitude)
{
  v.setLongitude(longitude);
}
 
double CoordinateValueType::longitude() const
{
  return v.longitude();
}
 
/*
  This property holds latitude value of the geographical position
  (decimal degrees). A positive latitude indicates the Northern Hemisphere,
  and a negative latitude indicates the Southern Hemisphere.
  If the property has not been set, its default value is NaN.
*/
void CoordinateValueType::setLatitude(double latitude)
{
  v.setLatitude(latitude);
}
 
double CoordinateValueType::latitude() const
{
  return v.latitude();
}
 
/*
  This property holds the current validity of the coordinate. Coordinates
  are considered valid if they have been set with a valid latitude and
  longitude (altitude is not required).
 
  The latitude must be between -90 to 90 inclusive to be considered valid,
  and the longitude must be between -180 to 180 inclusive to be considered
  valid.
*/
bool CoordinateValueType::isValid() const
{
  return v.isValid();
}
 
QString CoordinateValueType::toString() const
{
  return QStringLiteral("QGeoCoordinate(%1,%2,%3)")
    .arg(v.latitude()).arg(v.longitude()).arg(v.altitude());
}
 
bool CoordinateValueType::isEqual(const QVariant &other) const
{
  if (other.userType() != qMetaTypeId<QGeoCoordinate>())
    return false;
 
  return v == other.value<QGeoCoordinate>();
}
 
/*
  Returns the distance (in meters) from this coordinate to the
  coordinate specified by other. Altitude is not used in the calculation.
 
  This calculation returns the great-circle distance between the two
  coordinates, with an assumption that the Earth is spherical for the
  purpose of this calculation.
*/
qreal CoordinateValueType::distanceTo(const QGeoCoordinate &coordinate) const
{
  return v.distanceTo(coordinate);
}
 
/*
  Returns the azimuth (or bearing) in degrees from this coordinate to the
  coordinate specified by other. Altitude is not used in the calculation.
 
  There is an assumption that the Earth is spherical for the purpose of
  this calculation.
*/
qreal CoordinateValueType::azimuthTo(const QGeoCoordinate &coordinate) const
{
  return v.azimuthTo(coordinate);
}
 
/*
  Returns the coordinate that is reached by traveling distance metres
  from the current coordinate at azimuth degrees along a great-circle.
 
  There is an assumption that the Earth is spherical for the purpose
  of this calculation.
*/
QGeoCoordinate CoordinateValueType::atDistanceAndAzimuth(qreal distance, qreal azimuth) const
{
  return v.atDistanceAndAzimuth(distance, azimuth);
}
 
QT_END_NAMESPACE
