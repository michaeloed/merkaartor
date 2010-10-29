#include "TrackSegment.h"
#include "DocumentCommands.h"
#include "TrackSegmentCommands.h"
#include "MapView.h"
#include "Node.h"
#include "Utils/LineF.h"

#include <QtGui/QPainter>
#include <QProgressDialog>

#include <algorithm>
#include <QList>

#define TEST_RFLAGS(x) theView->renderOptions().options.testFlag(x)

class TrackSegmentPrivate
{
    public:
        TrackSegmentPrivate()
        : Distance(0)
        {
        }

        QList<Node*> Nodes;
        double Distance;
        CoordBox BBox;
};

TrackSegment::TrackSegment(void)
{
    p = new TrackSegmentPrivate;
    setRenderPriority(RenderPriority (RenderPriority::IsLinear,0.,99));
}

TrackSegment::TrackSegment(const TrackSegment& other)
: Feature(other)
{
    p = new TrackSegmentPrivate;
    setRenderPriority(RenderPriority (RenderPriority::IsLinear,0.,99));
}

TrackSegment::~TrackSegment(void)
{
    // TODO Those cleanup shouldn't be necessary and lead to crashes
    //      Check for side effect of supressing them.
//	for (int i=0; i<p->Nodes.size(); ++i)
//		p->Nodes[i]->unsetParentFeature(this);
    delete p;
}

void TrackSegment::sortByTime()
{
    for (int i=0; i<p->Nodes.size(); ++i)
    {
        for (int j=i+1; j<p->Nodes.size(); ++j)
        {
            if (p->Nodes[i]->time() > p->Nodes[j]->time())
            {
                QDateTime dt(p->Nodes[i]->time());
                p->Nodes[i]->setTime(p->Nodes[j]->time());
                p->Nodes[j]->setTime(dt);
            }
        }
    }
}

QString TrackSegment::description() const
{
    return "tracksegment";
}

void TrackSegment::add(Node* aPoint)
{
    p->Nodes.push_back(aPoint);
    aPoint->setParentFeature(this);
}

void TrackSegment::add(Node* Pt, int Idx)
{
    p->Nodes.push_back(Pt);
    std::rotate(p->Nodes.begin()+Idx,p->Nodes.end()-1,p->Nodes.end());
}

int TrackSegment::find(Feature* Pt) const
{
    for (int i=0; i<p->Nodes.size(); ++i)
        if (p->Nodes[i] == Pt)
            return i;
    return p->Nodes.size();
}

void TrackSegment::remove(int idx)
{
    Node* Pt = p->Nodes[idx];
    p->Nodes.erase(p->Nodes.begin()+idx);
    Pt->unsetParentFeature(this);
}

void TrackSegment::remove(Feature* F)
{
    for (int i=p->Nodes.size(); i; --i)
        if (p->Nodes[i-1] == F)
            remove(i-1);
}

int TrackSegment::size() const
{
    return p->Nodes.size();
}

Feature* TrackSegment::get(int i)
{
    return p->Nodes[i];
}

Node* TrackSegment::getNode(int i)
{
    return p->Nodes[i];
}

const Feature* TrackSegment::get(int Idx) const
{
    return p->Nodes[Idx];
}

bool TrackSegment::isNull() const
{
    return (p->Nodes.size() == 0);
}

void TrackSegment::drawDirectionMarkers(QPainter &P, QPen &pen, const QPointF & FromF, const QPointF & ToF)
{
    if (::distance(FromF,ToF) <= 30.0)
        return;

    const double DistFromCenter=10.0;
    const double theWidth = !M_PREFS->getSimpleGpxTrack() ? 5.0 : 8.0;
    const double A = angle(FromF-ToF);

    QPointF T(DistFromCenter*cos(A), DistFromCenter*sin(A));
    QPointF V1(theWidth*cos(A+M_PI/6),theWidth*sin(A+M_PI/6));
    QPointF V2(theWidth*cos(A-M_PI/6),theWidth*sin(A-M_PI/6));

    pen.setStyle(Qt::SolidLine);
    P.setPen(pen);

    QPointF H((FromF+ToF) / 2.0);
    P.drawLine(H-T,H-T+V1);
    P.drawLine(H-T,H-T+V2);
}

void TrackSegment::draw(QPainter &P, MapView* theView)
{
    QPen pen;

    if (!TEST_RFLAGS(RendererOptions::TrackSegmentVisible))
        return;

    for (int i=1; i<p->Nodes.size(); ++i)
    {
        Coord last = p->Nodes[i-1]->position();
        Coord here = p->Nodes[i]->position();

        if (CoordBox::visibleLine(theView->viewport(), last, here) == false)
            continue;

        QPointF FromF(theView->toView(last));
        QPointF ToF(theView->toView(here));

        if (!M_PREFS->getSimpleGpxTrack())
        {
            double distance = here.distanceFrom(last);
            double slope = (p->Nodes[i]->elevation() - p->Nodes[i-1]->elevation()) / (distance * 10.0);
            double speed = p->Nodes[i]->speed();

            int width = M_PREFS->getGpxTrackWidth();
            // Dynamic track line width adaption to zoom level
            if (theView->pixelPerM() > 2)
                width++;
            else if (theView->pixelPerM() < 1)
                width--;

            // Encode speed in width of path ...
            double penWidth = 1.0;
            if (speed > 10.0)
                penWidth = qMin(1.0+speed*0.02, 5.0);

            penWidth *= width;

            // ... and slope in the color
            int green = 0;
            int red = 0;

            if (slope > 2.0)
            {
                slope = qMin(slope, 20.0);
                green = 48 + int(slope*79.0 / 20.0);
            }
            else if (slope < -2.0)
            {
                slope = qMax(slope, - 20.0);
                red = 48 + int(-slope*79.0 / 20.0);
            }

            pen.setColor(QColor(128 + red, 128 + green, 128));

            pen.setStyle(Qt::DotLine);
            pen.setWidthF(penWidth);
        }
        else
        {
            int width = M_PREFS->getGpxTrackWidth();
            // Dynamic track line width adaption to zoom level
            if (theView->pixelPerM() > 2)
                width++;
            else if (theView->pixelPerM() < 1)
                width--;
            pen.setWidthF(width);
            pen.setColor(M_PREFS->getGpxTrackColor());
        }
        P.setPen(pen);

        P.drawLine(FromF,ToF);
        drawDirectionMarkers(P, pen, FromF, ToF);
    }
}

bool TrackSegment::notEverythingDownloaded()
{
    return false;
}

void TrackSegment::drawSpecial(QPainter&, QPen&, MapView*)
{
    // not implemented
}

void TrackSegment::drawParentsSpecial(QPainter&, QPen&, MapView*)
{
    // not implemented
}

void TrackSegment::drawChildrenSpecial(QPainter&, QPen&, MapView*, int)
{
    // not implemented
}


const CoordBox& TrackSegment::boundingBox(bool) const
{
    if (p->Nodes.size())
    {
        p->BBox = CoordBox(p->Nodes[0]->position(),p->Nodes[0]->position());
        for (int i=1; i<p->Nodes.size(); ++i)
            p->BBox.merge(p->Nodes[i]->position());
    } else
        p->BBox = CoordBox();
    return p->BBox;
}

double TrackSegment::pixelDistance(const QPointF& , double , bool , MapView*) const
{
    // unable to select that one
    return 1000000;
}

void TrackSegment::cascadedRemoveIfUsing(Document* theDocument, Feature* aFeature, CommandList* theList, const QList<Feature*>& Proposals)
{
    for (int i=0; i<p->Nodes.size();) {
        if (p->Nodes[i] == aFeature)
        {
            QList<Node*> Alternatives;
            for (int j=0; j<Proposals.size(); ++j)
            {
                Node* Pt = dynamic_cast<Node*>(Proposals[j]);
                if (Pt)
                    Alternatives.push_back(Pt);
            }
            if ( (p->Nodes.size() == 1) && (Alternatives.size() == 0) )
                theList->add(new RemoveFeatureCommand(theDocument,this));
            else
            {
                for (int j=0; j<Alternatives.size(); ++j)
                {
                    if (i < p->Nodes.size())
                    {
                        if (p->Nodes[i+j] != Alternatives[j])
                        {
                            if ((i+j) == 0)
                                theList->add(new TrackSegmentAddNodeCommand(this, Alternatives[j], i+j,Alternatives[j]->layer()));
                            else if (p->Nodes[i+j-1] != Alternatives[j])
                                theList->add(new TrackSegmentAddNodeCommand(this, Alternatives[j], i+j,Alternatives[j]->layer()));
                        }
                    }
                }
                theList->add(new TrackSegmentRemoveNodeCommand(this, (Node*)aFeature,aFeature->layer()));
            }
        }
        ++i;
    }
}

void TrackSegment::partChanged(Feature*, int)
{
}

void TrackSegment::updateMeta()
{
    Feature::updateMeta();

    p->Distance = 0;

    if (p->Nodes.size() == 0)
    {
        MetaUpToDate = true;
        return;
    }

    for (int i=0; (i+1)<p->Nodes.size(); ++i)
    {
        if (p->Nodes[i] && p->Nodes[i+1]) {
            const Coord & here = p->Nodes[i]->position();
            const Coord & next = p->Nodes[i+1]->position();

            p->Distance += next.distanceFrom(here);
        }
    }


    MetaUpToDate = true;
}

double TrackSegment::distance()
{
    if (MetaUpToDate == false)
        updateMeta();

    return p->Distance;
}

int TrackSegment::duration() const
{
    return p->Nodes[0]->time().secsTo(p->Nodes[p->Nodes.size() - 1]->time());
}


bool TrackSegment::toGPX(QDomElement xParent, QProgressDialog * progress, bool forExport)
{
    bool OK = true;

    QDomElement e = xParent.ownerDocument().createElement("trkseg");
    xParent.appendChild(e);

    if (!forExport)
        e.setAttribute("xml:id", xmlId());

    for (int i=0; i<size(); ++i) {
        dynamic_cast <Node*> (get(i))->toGPX(e, progress, forExport);
    }

    if (progress)
        progress->setValue(progress->value()+1);

    return OK;
}

bool TrackSegment::toXML(QDomElement xParent, QProgressDialog * progress, bool)
{
    return toGPX(xParent, progress, false);
}

TrackSegment* TrackSegment::fromGPX(Document* d, Layer* L, const QDomElement e, QProgressDialog * progress)
{
    TrackSegment* l = new TrackSegment();

    if (e.hasAttribute("xml:id"))
        l->setId(IFeature::FId(IFeature::GpxSegment, e.attribute("xml:id").toLongLong()));

    QDomElement c = e.firstChildElement();
    while(!c.isNull()) {
        if (c.tagName() == "trkpt") {
            Node* N = Node::fromGPX(d, L, c);
            l->add(N);
            progress->setValue(progress->value()+1);
        }

        if (progress->wasCanceled())
            break;

        c = c.nextSiblingElement();
    }

    return l;
}

TrackSegment* TrackSegment::fromXML(Document* d, Layer* L, const QDomElement e, QProgressDialog * progress)
{
    return TrackSegment::fromGPX(d, L, e, progress);
}
