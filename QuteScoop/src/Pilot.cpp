/**************************************************************************
 *  This file is part of QuteScoop. See README for license
 **************************************************************************/

#include "Pilot.h"

#include "PilotDetails.h"
#include "NavData.h"
#include "helpers.h"
#include "Settings.h"

Pilot::Pilot(const QStringList& stringList, const WhazzupData* whazzup):
    Client(stringList, whazzup),
    onGround(false),
    showDepDestLine(false)
{
    whazzupTime = QDateTime(whazzup->timestamp()); // need some local reference to that

    altitude = getField(stringList, 7).toInt(); // we could do some barometric calculations here (only for VATSIM needed)
    groundspeed = getField(stringList, 8).toInt();
    planAircraft = getField(stringList, 9);
    planTAS = getField(stringList, 10);
    planDep = getField(stringList, 11);
    planAlt = getField(stringList, 12);
    planDest = getField(stringList, 13);

    transponder = getField(stringList, 17);
    planRevision = getField(stringList, 20);
    planFlighttype = getField(stringList, 21);
    planDeptime = getField(stringList, 22);
    planActtime = getField(stringList, 23);

    QString tmpStr = getField(stringList, 24);
    if(tmpStr.isNull())
        planHrsEnroute = -1;
    else
        planHrsEnroute = tmpStr.toInt();
    planMinEnroute = getField(stringList, 25).toInt();
    planHrsFuel = getField(stringList, 26).toInt();
    planMinFuel = getField(stringList, 27).toInt();
    planAltAirport = getField(stringList, 28);
    planRemarks = getField(stringList, 29);
    planRoute = getField(stringList, 30);

    if(whazzup->isIvao()) {
        planAltAirport2 = getField(stringList, 42); // IVAO only
        planTypeOfFlight = getField(stringList, 43); // IVAO only
        pob = getField(stringList, 44).toInt(); // IVAO only

        trueHeading = getField(stringList, 45).toInt();
        onGround = getField(stringList, 46).toInt() == 1; // IVAO only
    }

    if(whazzup->isVatsim()) {
        trueHeading = getField(stringList, 38).toInt();
        qnhInHg = getField(stringList, 39); // VATSIM only
        qnhMb = getField(stringList, 40); // VATSIM only
    }
    // day of flight
    if(!QTime::fromString(planDeptime, "HHmm").isValid()) // no Plan ETA given: maybe some more magic needed here
        dayOfFlight = whazzupTime.date();
    else if((QTime::fromString(planDeptime, "HHmm").hour() - whazzupTime.time().hour()) % 24 <= 2) // allow for early birds (up to 2 hours before planned departure)
        dayOfFlight = whazzupTime.date();
    else
        dayOfFlight = whazzupTime.date().addDays(-1); // started the day before

    // anti-idiot hack: some guys like routes like KORLI/MARPI/UA551/FOF/FUN...
    if(planRoute.count("/") > 4)
        planRoute.replace(QRegExp("[/]"), " ");

    if(planRoute.isEmpty())
        planRoute = "";
}

void Pilot::showDetailsDialog() {
    PilotDetails *infoDialog = PilotDetails::getInstance(true);
    infoDialog->refresh(this);
    infoDialog->show();
    infoDialog->raise();
    infoDialog->setFocus();
    infoDialog->activateWindow();
}

Pilot::FlightStatus Pilot::flightStatus() const {
    Airport *dep = depAirport();
    Airport *dst = destAirport();

    // flying?
    bool flying = true;
    if(groundspeed < 50 && altitude < 9000) flying = false;
    if(onGround) flying = false;

    if ( dep == NULL || dst == NULL ) {
        if(flying)
            return EN_ROUTE;
        else
            return BUSH;
    }

    double totalDist = NavData::distance(dep->lat, dep->lon, dst->lat, dst->lon);
    double distDone = NavData::distance(lat, lon, dep->lat, dep->lon);
    double distRemaining = totalDist - distDone;

    // arriving?
    bool arriving = false;
    if(distRemaining < 50) arriving = true;

    // departing?
    bool departing = false;
    if(distDone < 50) departing = true;


    // BOARDING: !flying, speed = 0, departing
    // GROUND_DEP: !flying, speed > 0, departing
    // DEPARTING: flying, departing
    // EN_ROUTE: flying, !departing, !arriving
    // ARRIVING: flying, arriving
    // GROUND_ARR: !flying, speed > 0, arriving
    // BLOCKED: !flying, speed = 0, arriving
    // PREFILED: !flying, lat=0, lon=0

    if(!flying && groundspeed == 0 && departing)
        return BOARDING;
    if(!flying && groundspeed > 0 && departing)
        return GROUND_DEP;
    if(flying && arriving)
        return ARRIVING; // put before departing; on small hops tend to show "arriving", not departing
    if(flying && departing)
        return DEPARTING;
    if(flying && !departing && !arriving)
        return EN_ROUTE;
    if(!flying && groundspeed > 0 && arriving)
        return GROUND_ARR;
    if(!flying && qFuzzyIsNull(lat) && qFuzzyIsNull(lon)) // must be before BLOCKED
        return PREFILED;
    if(!flying && groundspeed == 0 && arriving)
        return BLOCKED;
    return CRASHED;
}

QString Pilot::flightStatusString() const {
    QString result;
    switch(flightStatus()) {
        case BOARDING:
            result = QString();
            result += "TTG " + eet().toString("H:mm") + " hrs";
            result += ", ETA " + eta().toString("HHmm") + "z";
            result += (delayStr().isEmpty()? "": ", Delay " + delayStr() + " hrs");
            return result;
        case GROUND_DEP:
            result = QString();
            result += "TTG " + eet().toString("H:mm") + " hrs";
            result += ", ETA " + eta().toString("HHmm") + "z";
            result += (delayStr().isEmpty()? "": ", Delay " + delayStr() + " hrs");
            return result;
        case DEPARTING:
            result = QString();
            result += "TTG " + eet().toString("H:mm") + " hrs";
            result += ", ETA " + eta().toString("HHmm") + "z";
            result += (delayStr().isEmpty()? "": ", Delay " + delayStr() + " hrs");
            return result;
        case ARRIVING:
            result = QString();
            result += "TTG " + eet().toString("H:mm") + " hrs";
            result += ", ETA " + eta().toString("HHmm") + "z";
            result += (delayStr().isEmpty()? "": ", Delay " + delayStr() + " hrs");
            return result;
        case GROUND_ARR:
            result = QString();
            result += "ATA " + eta().toString("HHmm") + "z"; // Actual Time of Arrival :)
            result += (delayStr().isEmpty()? "": ", Delay " + delayStr() + " hrs");
            return result;
        case BLOCKED:
            result = QString();
            result += "ATA " + eta().toString("HHmm") + "z"; // Actual Time of Arrival :)
            result += (delayStr().isEmpty()? "": ", Delay " + delayStr() + " hrs");
            return result;
        case CRASHED: return QString();
        case BUSH: return QString();
        case EN_ROUTE: {
                result = QString();
                Airport *dep = depAirport();
                Airport *dst = destAirport();
                if(dst == 0)
                    return QString();
                if(dep != 0) {
                    // calculate %done
                    int total_dist = (int)NavData::distance(dep->lat, dep->lon, dst->lat, dst->lon);
                    if(total_dist > 0) {
                        int dist_done = (int)NavData::distance(lat, lon, dep->lat, dep->lon);
                        result += QString("%1%").arg(dist_done * 100 / total_dist);
                    }
                }
                result += ", TTG " + eet().toString("H:mm") + " hrs";
                result += ", ETA " + eta().toString("HHmm") + "z";
                result += (delayStr().isEmpty()? "": ", Delay " + delayStr() + " hrs");
                return result;
            }
        case PREFILED:
            return QString();
    }
    return QString("Unknown");
}

QString Pilot::flightStatusShortString() const {
    switch(flightStatus()) {
        case BOARDING: return QString("Boarding");
        case GROUND_DEP: return QString("Taxi to runway");
        case DEPARTING: return QString("Departing");
        case ARRIVING: return QString("Arriving");
        case GROUND_ARR: return QString("Taxi to gate");
        case BLOCKED: return QString("Blocked at gate");
        case CRASHED: return QString("Crashed");
        case BUSH: return QString("Bush pilot");
        case EN_ROUTE: return QString("En Route");
        case PREFILED: return QString("Prefiled");
    }
    return QString("Unknown");
}

QString Pilot::planFlighttypeString() const {
    if (planFlighttype == "I")
        return QString("IFR");
    else if (planFlighttype == "V")
        return QString("VFR");
    else if (planFlighttype == "Y")
        return QString("Y: IFR to VFR");
    else if (planFlighttype == "Z")
        return QString("Z: VFR to IFR");
    else if (planFlighttype == "S")
        return QString("SVFR");
    else
        return QString(planFlighttype);
}

QString Pilot::rank() const {
    if(network == IVAO) {
        switch(rating) {
        case 2: return "SFO"; break;
        case 3: return "FFO"; break;
        case 4: return "C"; break;
        case 5: return "FC"; break;
        case 6: return "SC"; break;
        case 7: return "SFC"; break;
        case 8: return "CC"; break;
        case 9: return "CFC"; break;
        case 10: return "CSC"; break;
        default: return QString("? (%1)").arg(rating); break;
        }
    }
    return QString();
}

QString Pilot::aircraftType() const {
    QStringList acftSegments = planAircraft.split("/");

    if(network == IVAO && acftSegments.size() >= 2) {
        return acftSegments[1];
    }

    if(network == VATSIM) {
        // VATSIM can be a real PITA.
        QString result = planAircraft;
        if(acftSegments.size() == 2 && acftSegments[0].length() > 2) result = acftSegments[0];
        else if(acftSegments.size() >= 2) result = acftSegments[1];

        return result;
    }

    return planAircraft;
}

Airport* Pilot::depAirport() const {
    if(NavData::getInstance()->airports().contains(planDep))
        return NavData::getInstance()->airports()[planDep];
    else return 0;
}

Airport* Pilot::destAirport() const {
    if(NavData::getInstance()->airports().contains(planDest))
        return NavData::getInstance()->airports()[planDest];
    else return 0;
}

Airport* Pilot::altAirport() const {
    if(NavData::getInstance()->airports().contains(planAltAirport))
        return NavData::getInstance()->airports()[planAltAirport];
    else return 0;
}

double Pilot::distanceFromDeparture() const {
    Airport *dep = depAirport();
    if(dep == 0)
        return 0;

    return NavData::distance(lat, lon, dep->lat, dep->lon);
}

double Pilot::distanceToDestination() const {
    Airport *dest = destAirport();
    if(dest == 0)
        return 0;

    return NavData::distance(lat, lon, dest->lat, dest->lon);
}

int Pilot::planTasInt() const { // defuck flightplanned TAS
    if(planTAS.startsWith("M")) { // approximate Mach -> TAS conversion
        if(planTAS.contains("."))
            return (int) planTAS.mid(1).toDouble() * 550;
        else
            return static_cast<int>(planTAS.mid(1).toInt() * 5.5);
    }
    return planTAS.toInt();
}

QDateTime Pilot::etd() const { // Estimated Time of Departure
    if(planDeptime.length() < 3)
        return QDateTime();
    QString planDeptimeFixed = planDeptime;

    if(planDeptime.length() == 3)
        planDeptimeFixed.prepend("0"); // fromString("Hmm") does not handle "145" correctly -> "14:05"

    QTime time = QTime::fromString(planDeptimeFixed, "HHmm");
    return QDateTime(dayOfFlight, time, Qt::UTC);
}

QDateTime Pilot::eta() const { // Estimated Time of Arrival
    FlightStatus status = flightStatus();
    if(status == PREFILED || status == BOARDING) {
        return etaPlan();
    } else if(status == DEPARTING || status == GROUND_DEP) { // try to calculate with flightplanned speed
        int enrouteSecs;
        if(planTasInt() == 0) {
            if(groundspeed == 0)
                return QDateTime(); // abort
            enrouteSecs = (int) (distanceToDestination() * 3600) / groundspeed;
        } else {
            enrouteSecs = (int) (distanceToDestination() * 3600) / planTasInt();
        }
        if(status == GROUND_DEP) enrouteSecs += 240; // taxi time outbound
        return whazzupTime.addSecs(enrouteSecs);
    } else if(status == EN_ROUTE || status == ARRIVING) { // try groundspeed
        int enrouteSecs;
        if(groundspeed == 0) {
            if(planTasInt() == 0)
                return QDateTime(); // abort
            enrouteSecs = (int) (distanceToDestination() * 3600) / planTasInt();
        } else {
            enrouteSecs = (int) (distanceToDestination() * 3600) / groundspeed;
        }
        QDateTime ret = whazzupTime.addSecs(enrouteSecs);
        return ret;
    } else if(status == GROUND_ARR || status == BLOCKED) {
        return whazzupTime;
    }
    return QDateTime();
}

QTime Pilot::eet() const { // Estimated Enroute Time remaining
    int secs = whazzupTime.secsTo(eta());
    QTime ret = QTime((secs / 3600) % 24, (secs / 60) % 60);
    return ret;
}

QDateTime Pilot::etaPlan() const { // Estimated Time of Arrival as flightplanned
    if(planHrsEnroute < 0)
        return QDateTime();
    return etd().addSecs(planHrsEnroute * 3600 + planMinEnroute * 60);
}

QString Pilot::delayStr() const { // delay
    if(!etaPlan().isValid())
        return QString();
    int secs, calcSecs;
    secs = etaPlan().secsTo(eta());
    if (secs == 0)
        return QString("none");
    calcSecs = (secs < 0? -secs: secs);
    return QString("%1%2")
            .arg(secs < 0? "-": "")
            .arg(QTime((calcSecs / 3600) % 24, (calcSecs / 60) % 60).toString("H:mm"));
}

int Pilot::defuckPlanAlt(QString altStr) const { // returns an altitude from various flightplan strings
    altStr = altStr.trimmed();
    if(altStr.length() < 4 && altStr.toInt() != 0) // 280
        return altStr.toInt() * 100;
    if(altStr.left(2) == "FL") // FL280
        return altStr.mid(2).toInt() * 100;
    if(altStr.left(1) == "F") // F280
        return altStr.mid(1).toInt() * 100;
    if(altStr.left(1) == "A" && altStr.length() <= 4) // A45
        return altStr.mid(1).toInt() * 100;
    if(altStr.left(1) == "A" && altStr.length() > 4) // A4500
        return altStr.mid(1).toInt();
    return altStr.toInt();
}

QStringList Pilot::waypoints() const {
    QStringList result = planRoute.split(QRegExp("([\\s\\+\\-\\.\\,]|//)"), QString::SkipEmptyParts);
    if(result.isEmpty())
        result.append("");
    return result;
}

QPair<double, double> Pilot::positionInFuture(int seconds) const {
    double dist = (double)groundspeed * ((double)seconds / 3600.0);
    return NavData::pointDistanceBearing(lat, lon, dist, trueHeading);
}

void Pilot::plotDepDestLine() {
    //if (qFuzzyIsNull(lat) && qFuzzyIsNull(lon))
    //    return; // some lines go to/come from N0/E0. This might be a prefiled flight or some other error.
    QList<Waypoint*> waypoints = routeWaypoints();
    if(depAirport() != 0)
        waypoints.prepend(new Waypoint(depAirport()->label, depAirport()->lat, depAirport()->lon));
    if(destAirport() != 0)
        waypoints.append(new Waypoint(destAirport()->label, destAirport()->lat, destAirport()->lon));
    if(waypoints.size() < 1) // either dep or dest should be known
        return;

    int nextPoint = qMin(1, waypoints.size()); // find the nextPoint on his route after the present plane position
    // calculate first point behind plane
    if(qFuzzyIsNull(lat) && qFuzzyIsNull(lon)) {
        nextPoint = 1; // prefiled flight or no known position
    } else {
        // find the point that is nearest to the plane
        double minDist = NavData::distance(lat, lon, waypoints[0]->lat, waypoints[0]->lon);
        int minPoint = 0; // next to departure as default
        for(int i = 1; i < waypoints.size(); i++) {
            if(NavData::distance(lat, lon, waypoints[i]->lat, waypoints[i]->lon) < minDist) {
                minDist = NavData::distance(lat, lon, waypoints[i]->lat, waypoints[i]->lon);
                minPoint = i;
            }
        }

        // with the nearest point, look which one is the next point ahead - saves from trouble with zig-zag routes
        if(minPoint == 0) {
            nextPoint = 1;
        } else if(minPoint == waypoints.size() - 1) {
            nextPoint = waypoints.size() - 1;
        } else {
            nextPoint = minPoint + 1; // default
            // look for the first route segment where the planned course deviates > 90° from the bearing to the plane
            int courseRoute, courseToPlane, courseDeviation;
            for(int i = minPoint - 1; i <= minPoint; i++) {
                courseRoute = (int) NavData::courseTo(waypoints[i]->lat, waypoints[i]->lon, waypoints[i + 1]->lat, waypoints[i + 1]->lon);
                courseToPlane = (int) NavData::courseTo(waypoints[i]->lat, waypoints[i]->lon, lat, lon);
                courseDeviation = (qAbs(courseRoute - courseToPlane)) % 360;
                if (courseDeviation > 90) {
                    nextPoint = i;
                    break;
                }
            }
        }
    }

    QList<DoublePair> points; // these are the points that really get drawn
    if (depAirport() != 0) { // Dep -> plane
        if (Settings::depLineStrength() > 0. && (showDepDestLine || depAirport()->showFlightLines)) {
            for (int i = 0; i < nextPoint; i++)
                points.append(DoublePair(waypoints[i]->lat, waypoints[i]->lon));
        }
    }

    if (!qFuzzyIsNull(lat) && !qFuzzyIsNull(lon)) { // plane ok: draw to plane and reset list for DestLine
        points.append(DoublePair(lat, lon));

        if (Settings::depLineDashed())
            glLineStipple(3, 0xAAAA);
        QColor lineCol = Settings::depLineColor();
        glColor4f(lineCol.redF(), lineCol.greenF(), lineCol.blueF(), lineCol.alphaF());
        glLineWidth(Settings::depLineStrength());
        glBegin(GL_LINE_STRIP);
        NavData::plotPointsOnEarth(points);
        glEnd();
        if(Settings::depLineDashed())
            glLineStipple(1, 0xFFFF);

        points.clear();
        points.append(DoublePair(lat, lon));
    }

    if (destAirport() != 0) { // plane -> Dest
        if (Settings::destLineStrength() > 0. && (showDepDestLine || destAirport()->showFlightLines)) {
            for (int i = nextPoint; i < waypoints.size(); i++)
                points.append(DoublePair(waypoints[i]->lat, waypoints[i]->lon));
            if (Settings::destLineDashed())
                glLineStipple(3, 0xAAAA);
            QColor lineCol = Settings::destLineColor();
            glColor4f(lineCol.redF(), lineCol.greenF(), lineCol.blueF(), lineCol.alphaF());
            glLineWidth(Settings::destLineStrength());
            glBegin(GL_LINE_STRIP);
            NavData::plotPointsOnEarth(points);
            glEnd();
            if(Settings::destLineDashed())
                glLineStipple(1, 0xFFFF);
        }
    }
}


QList<Waypoint*> Pilot::routeWaypoints() {
    //qDebug() << "Pilot::routeWaypoints()" << label;
    if ((planDep == routeWaypointsPlanDepCache) // we might have cached the route already
        && (planDest == routeWaypointsPlanDestCache)
        && (planRoute == routeWaypointsPlanRouteCache)) {
        return routeWaypointsCache; // no changes
    }

    Airport *dep = depAirport();
    routeWaypointsPlanDepCache = planDep;
    routeWaypointsPlanDestCache = planDest;
    routeWaypointsPlanRouteCache = planRoute;

    if (dep != 0) {
        routeWaypointsCache = NavData::getInstance()->getAirac().resolveFlightplan(
                waypoints(), dep->lat, dep->lon);
    } else
        routeWaypointsCache = QList<Waypoint*>();

    //qDebug() << "Pilot::routeWaypoints() -- finished" << label;
    return routeWaypointsCache;
}
