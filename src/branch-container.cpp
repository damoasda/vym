#include <QDebug>
#include <math.h>

#include "branch-container.h"

#include "branchitem.h"
#include "flag-container.h"
#include "flagrow-container.h"
#include "frame-container.h"
#include "geometry.h"
#include "heading-container.h"
#include "link-container.h"
#include "linkobj.h"
#include "mapdesign.h"
#include "misc.h"
#include "xlinkobj.h"

#define qdbg() qDebug().nospace().noquote()

extern FlagRowMaster *standardFlagsMaster;
extern FlagRowMaster *userFlagsMaster;
extern FlagRowMaster *systemFlagsMaster;

qreal BranchContainer::linkWidth = 20;  // FIXME-3 testing

BranchContainer::BranchContainer(QGraphicsScene *scene, BranchItem *bi)  // FIXME-2 scene and addItem should not be required, only for mapCenters without parent:  setParentItem automatically sets scene!
{
    // qDebug() << "* Const BranchContainer begin this = " << this << "  branchitem = " << bi;
    scene->addItem(this);
    branchItem = bi;
    init();
}

BranchContainer::~BranchContainer()
{
    //qDebug() << "* Destr BranchContainer" << getName() << this;

    if (branchItem)
    {
        // Unlink all containers in my own subtree, which will be deleted
        // when tree of QGraphicsItems is deleted.
        // In every destructor tell the linked BranchItem to longer consider deleting containers
    // when the BranchItem will be deleted later
        branchItem->unlinkBranchContainer();
    }

    if (upLink)
        delete upLink;
}

void BranchContainer::init()
{
    containerType = Container::Branch;

    // BranchContainers inherit FrameContainer, reset overlay
    overlay = false;

    orientation = UndefinedOrientation;

    imagesContainer = nullptr;
    selectionContainer = nullptr;

    headingContainer = new HeadingContainer;

    innerFrame = nullptr;
    outerFrame = nullptr;

    ornamentsContainer = new Container;
    ornamentsContainer->containerType = OrnamentsContainer;

    linkContainer = new LinkContainer;

    innerContainer = new Container;
    innerContainer->containerType = InnerContainer;

    standardFlagRowContainer = nullptr;
    systemFlagRowContainer = nullptr;

    listContainer = nullptr;
    bulletPointContainer = nullptr;

    ornamentsContainer->addContainer(headingContainer, Z_HEADING);

    ornamentsContainer->setCentralContainer(headingContainer);

    innerContainer->addContainer(linkContainer, Z_LINK);
    innerContainer->addContainer(ornamentsContainer, Z_ORNAMENTS);

    branchesContainer = nullptr;
    linkSpaceContainer = nullptr;

    outerContainer = nullptr;

    addContainer(innerContainer);

    setLayout(Container::Horizontal);
    setHorizontalDirection(Container::LeftToRight);

    // Create an uplink for every branch 
    // This link will become the child of my parents
    // linkContainer later. This will allow moving when parent moves,
    // without recalculating geometry.
    upLink = new LinkObj;

    // Center of whole mainBranches should be the heading
    setCentralContainer(headingContainer);

    // Set some defaults, should be overridden from MapDesign later
    branchesContainerAutoLayout = true;
    branchesContainerLayout = Vertical;

    imagesContainerAutoLayout = true;
    imagesContainerLayout = FloatingFree;

    // Not temporary linked
    tmpLinkedParentContainer = nullptr;
    originalParentBranchContainer = nullptr;

    scrollOpacity = 1;

    rotationHeadingInt = 0;
    rotationSubtreeInt = 0;

    autoDesignInnerFrame = true;
    autoDesignOuterFrame = true;
}

BranchContainer* BranchContainer::parentBranchContainer()
{
    if (!branchItem)
        return nullptr;

    // For MapCenters parentBranch is rootItem and will return nullptr
    return branchItem->parentBranch()->getBranchContainer();
}

void BranchContainer::setBranchItem(BranchItem *bi) { branchItem = bi; }

BranchItem *BranchContainer::getBranchItem() const { return branchItem; }

QString BranchContainer::getName() {
    if (branchItem)
        return Container::getName() + " '" + branchItem->getHeadingPlain() + "'";
    else
        return Container::getName() + " - ?";
}

void BranchContainer::setOrientation(const Orientation &o)
{
    orientation = o;
}

void BranchContainer::setOriginalOrientation()  // FIXME-1 refactor the setOrig* methods into one setTmpParentContainer()
{
    originalOrientation = orientation;
    originalFloating = isFloating();
    originalParentBranchContainer = parentBranchContainer();
    if (parentItem()) {
        /*
        qDebug() << "BC:setOrient of " << info();
        qDebug() << "        parent: " << parentBranchContainer();
        qDebug() << "        parent: " << parentBranchContainer()->info() <<originalParentPos;
        */
        originalParentPos = parentBranchContainer()->downLinkPos();
    }
}

BranchContainer::Orientation BranchContainer::getOriginalOrientation()
{
    return originalOrientation;
}

BranchContainer::Orientation BranchContainer::getOrientation()
{
    return orientation;
}

QPointF BranchContainer::getOriginalParentPos()
{
    return originalParentPos;
}

#include <QTransform>
void BranchContainer::setScrollOpacity(qreal o)   // FIXME-2 testing for potential later animation
{
    scrollOpacity = o;
    //headingContainer->setScrollOpacity(a);
    headingContainer->setOpacity(o);
    if (standardFlagRowContainer)
        standardFlagRowContainer->setOpacity(o);
    QTransform t;
    t.scale (1, o);
    setTransform(t);

    qDebug() << "BC::sSO  o=" << o << " flags=" << flags();
    setOpacity(o);
}

qreal BranchContainer::getScrollOpacity()
{
    return scrollOpacity;
}

bool BranchContainer::isOriginalFloating()
{
    return originalFloating;
}

void BranchContainer::setTemporaryLinked(BranchContainer *tpc)
{
    tmpLinkedParentContainer = tpc;
    if (containerType != TmpParent)
        updateUpLink();
}

void BranchContainer::unsetTemporaryLinked()
{
    tmpLinkedParentContainer = nullptr;
    if (containerType != TmpParent)
        updateUpLink();
}

bool BranchContainer::isTemporaryLinked()
{
    if (tmpLinkedParentContainer)
        return true;
    else
        return false;
}

int BranchContainer::childrenCount()
{
    return branchCount() + imageCount();
}

int BranchContainer::branchCount()
{
    if (!branchesContainer)
        return 0;
    else
        return branchesContainer->childItems().count();
}

bool BranchContainer::hasFloatingBranchesLayout()
{
    if (branchesContainer)
        return branchesContainer->hasFloatingLayout();

    if (branchesContainerLayout == FloatingBounded || branchesContainerLayout == FloatingFree)
        return true;
    else
        return false;
}

bool BranchContainer::hasFloatingImagesLayout()
{
    if (imagesContainer)
        return imagesContainer->hasFloatingLayout();

    if (imagesContainerLayout == FloatingBounded || imagesContainerLayout == FloatingFree)
        return true;
    else
        return false;
}


void BranchContainer::addToBranchesContainer(Container *c, bool keepScenePos)
{
    if (!branchesContainer) {
        // Create branchesContainer before adding to it
        // (It will be deleted later in updateChildrenStructure(), if there
        // are no children)
        branchesContainer = new Container ();
        branchesContainer->containerType = Container::BranchesContainer;
        branchesContainer->zPos = Z_BRANCHES;

        if (listContainer)
            listContainer->addContainer(branchesContainer);
        else
            innerContainer->addContainer(branchesContainer);

    }

    QPointF sp = c->scenePos();
    branchesContainer->addContainer(c);

    updateBranchesContainerLayout();

    if (keepScenePos)
        c->setPos(branchesContainer->sceneTransform().inverted().map(sp));
}

Container* BranchContainer::getBranchesContainer()
{
    return branchesContainer;
}

void BranchContainer::updateImagesContainer()
{
    if (imagesContainer && imagesContainer->childItems().count() == 0) {
        delete imagesContainer;
        imagesContainer = nullptr;
    }
}

void BranchContainer::createOuterContainer()
{
    if (!outerContainer) {
        outerContainer = new Container;
        outerContainer->containerType = OuterContainer;
        outerContainer->setLayout(BoundingFloats);
        addContainer(outerContainer);

        // Children structure is updated in updateChildrenStructure(), which is
        // anyway calling this method
    }
}

void BranchContainer::deleteOuterContainer()
{
    if (outerContainer) {
        // Before outerContainer get's deleted, it's children need to be reparented
        if (outerFrame)
            outerFrame->addContainer(innerContainer);
        else
            addContainer(innerContainer);
        if (imagesContainer)
            innerContainer->addContainer(imagesContainer);

        delete outerContainer;
        outerContainer = nullptr;
    }
}

void BranchContainer::updateChildrenStructure()
                                                // When a map with list layout is loaded and 
                                                // layout is switched to e.g. Vertical, the links 
                                                // are not drawn. Has to be saved/loaded first
{
    // The structure of subcontainers within a BranchContainer
    // depends on layouts of imagesContainer and branchesContainer:
    //
    // Usually both inagesContainer and branchesContainer are children of
    // innerContainer.  The layout of innerContainer is either Horizontal or
    // BoundingFloats. outerContainer is only needed in corner case d)
    //
    // a) No FloatingBounded children
    //    - No outerContainer
    //    - innerContainer is Horizontal
    //    - branchesContainer is not FloatingBounded
    //    - imagesContainer is FloatingFree
    //
    // b) Only branches are FloatingBounded
    //    - No outerContainer
    //    - innerContainer BoundingFloats
    //    - branchesContainer is FloatingBounded
    //    - imagesContainer is FloatingFree
    //
    // c) images and branches are FloatingBounded
    //    - No outerContainer
    //    - innerContainer BoundingFloats
    //    - branchesContainer is FloatingBounded
    //    - imagesContainer is FloatingBounded
    //
    // d) Only images are FloatingBounded
    //    - outerContainer contains
    //      - innerContainer
    //      - imagesContainer
    //    - innerContainer is Horizontal
    //    - branchesContainer is Vertical
    //    - imagesContainer is FloatingBounded

    /*
    QString h;
    if (branchItem)
        h = branchItem->getHeadingPlain();
    qDebug() << "BC::updateChildrenStructure() of " << h;
    */

    if (branchesContainerLayout != FloatingBounded && imagesContainerLayout != FloatingBounded) {
        // a) No FloatingBounded images or branches
        deleteOuterContainer();
    } else if (branchesContainerLayout == FloatingBounded && imagesContainerLayout != FloatingBounded) {
        // b) Only branches are FloatingBounded
        deleteOuterContainer();
        innerContainer->setLayout(BoundingFloats);
    } else if (branchesContainerLayout == FloatingBounded && imagesContainerLayout == FloatingBounded) {
        // c) images and branches are FloatingBounded
        deleteOuterContainer();
        innerContainer->setLayout(BoundingFloats);
    } else if (branchesContainerLayout != FloatingBounded && imagesContainerLayout == FloatingBounded) {
        // d) Only images are FloatingBounded
        createOuterContainer();
        if (listContainer)
            innerContainer->setLayout(Vertical);
        else
            innerContainer->setLayout(Horizontal);
    } else {
        // e) remaining cases
        deleteOuterContainer();
        innerContainer->setLayout(FloatingBounded);
    }

    // Rotation of outer container or outerFrame    // FIXME-2 optimize and set only on demand? Maybe in updateStyles()?
    if (outerFrame) {
        outerFrame->setRotation(rotationSubtreeInt);
        if (outerContainer)
            outerContainer->setRotation(0);
        else
            innerContainer->setRotation(0);
    } else {
        if (outerContainer)
            outerContainer->setRotation(rotationSubtreeInt);
        else
            innerContainer->setRotation(rotationSubtreeInt);
    }

    // Rotation of heading
    if (innerFrame)
        innerFrame->setRotation(rotationHeadingInt);
    else
        ornamentsContainer->setRotation(rotationHeadingInt);

    // Update structure of outerContainer
    if (outerContainer) {
        // outerContainer should be child of outerFrame, if this is used
        if (outerFrame)
            outerFrame->addContainer(outerContainer);
        else
            outerContainer->setParentItem(this);
        outerContainer->addContainer(innerContainer);
        if (imagesContainer)
            outerContainer->addContainer(imagesContainer);
    }

    // Structure for bullet point list layouts
    BranchContainer *pbc = parentBranchContainer();
    if (pbc && pbc->branchesContainerLayout == List) {
        // Parent has list layout
        if (!bulletPointContainer) {
            //qDebug() << "... Creating bulletPointContainer";
            bulletPointContainer = new HeadingContainer;    // FIXME-2 create new type or re-use LinkObj and set type 
            // See also https://www.w3schools.com/charsets/ref_utf_punctuation.asp
            bulletPointContainer->setHeading(" • ");
            bulletPointContainer->setHeadingColor(headingContainer->getHeadingColor());
            ornamentsContainer->addContainer(bulletPointContainer, Z_BULLETPOINT);
        }
    } else {
        // Parent has no list layout
        if (bulletPointContainer) {
            delete bulletPointContainer;
            bulletPointContainer = nullptr;
        }
    }

    if (branchesContainerLayout == List) {
        if (!listContainer) {
            // Create and setup a listContainer *below* the ornamentsContainer
            innerContainer->setLayout(Vertical);
            innerContainer->setVerticalAlignment(AlignedLeft);

            // listContainer has one linkSpaceCOntainer left of branchesContainer
            listContainer = new Container;
            listContainer->containerType = Container::ListContainer;
            listContainer->setLayout(Horizontal);
            if (linkSpaceContainer)
                listContainer->addContainer(linkSpaceContainer);
            if (branchesContainer)
                listContainer->addContainer(branchesContainer);

            // Insert at end, especially behind innerFrame or ornamentsContainer
            innerContainer->addContainer(listContainer, Z_LIST);
            //qDebug() << "... Created listContainer in branch " << branchItem->getHeadingPlain();

        }
    } else {
        // Switch back from listContainer to regular setup with innerContainer
        if (listContainer) {
            innerContainer->setLayout(Horizontal);
            if (linkSpaceContainer)
                innerContainer->addContainer(linkSpaceContainer);
            if (branchesContainer)
                innerContainer->addContainer(branchesContainer);
            delete listContainer;
            listContainer = nullptr;
        }
    }

    updateImagesContainer();

    if (branchCount() == 0) {
        // no children branches, remove unused containers
        if (linkSpaceContainer) {
            delete linkSpaceContainer;
            linkSpaceContainer = nullptr;
        }
        if (branchesContainer) {
            delete branchesContainer;
            branchesContainer = nullptr;
        }
    } else {
        if (!branchesContainer) {
            // should never happen
            qDebug() << "BC::updateBranchesContainer no branchesContainer available!";
            return;
        }

        // Space for links depends on layout and scrolled state:
        if (linkSpaceContainer) {
            if (hasFloatingBranchesLayout() || branchItem->isScrolled()) {
                delete linkSpaceContainer;
                linkSpaceContainer = nullptr;
            }
        } else {
            if (!hasFloatingBranchesLayout() && !branchItem->isScrolled()) {
                linkSpaceContainer = new HeadingContainer ();
                linkSpaceContainer->setContainerType(LinkSpace);
                linkSpaceContainer->zPos = Z_LINKSPACE;
                linkSpaceContainer->setHeading("   ");  // FIXME-2 introduce minWidth later in Container instead of a pseudo heading here  see oc.pos

                if (listContainer)
                    listContainer->addContainer(linkSpaceContainer);
                else
                    innerContainer->addContainer(linkSpaceContainer);

                linkSpaceContainer->stackBefore(branchesContainer);
            }
        }
    }
}

int BranchContainer::imageCount()
{
    if (!imagesContainer)
        return 0;
    else
        return imagesContainer->childItems().count();
}

void BranchContainer::createImagesContainer()
{
    // imagesContainer is created when images are added to branch
    // The destructor of ImageItem calls 
    // updateChildrenStructure() in parentBranch()
    imagesContainer = new Container ();
    imagesContainer->containerType = ImagesContainer;
    imagesContainer->setLayout(imagesContainerLayout);

    if (outerContainer)
        outerContainer->addContainer(imagesContainer);
    else
        innerContainer->addContainer(imagesContainer);
}

void BranchContainer::addToImagesContainer(Container *c, bool keepScenePos)
{
    if (!imagesContainer) {
        createImagesContainer();
        /* FIXME-2 imagesContainer styles should be updated in Container c, but ImageContainer has no updateStyles() yet
        if (branchItem)
            updateStyles(RelinkBranch);
        */
    }

    QPointF sp = c->scenePos();
    imagesContainer->addContainer(c, Z_IMAGE);

    if (keepScenePos)
        c->setPos(imagesContainer->sceneTransform().inverted().map(sp));
}

Container* BranchContainer::getImagesContainer()
{
    return imagesContainer;
}

HeadingContainer* BranchContainer::getHeadingContainer()
{
    return headingContainer;
}

LinkContainer* BranchContainer::getLinkContainer()
{
    return linkContainer;
}

LinkObj* BranchContainer::getLink()
{
    return upLink;
}

void BranchContainer::linkTo(BranchContainer *pbc)
{

    if (!pbc) return;

    originalParentBranchContainer = nullptr;

    pbc->linkContainer->addLink(upLink);
}

QList <BranchContainer*> BranchContainer::childBranches()
{
    QList <BranchContainer*> list;

    if (!branchesContainer) return list;

    foreach (QGraphicsItem *g_item, branchesContainer->childItems())
        list << (BranchContainer*)g_item;

    return list;
}

QList <ImageContainer*> BranchContainer::childImages()
{
    QList <ImageContainer*> list;

    if (!imagesContainer) return list;

    foreach (QGraphicsItem *g_item, imagesContainer->childItems())
        list << (ImageContainer*)g_item;

    return list;
}

QPointF BranchContainer::getPositionHintNewChild(Container *c)
{
    // Should we put new child c on circle around myself?
    bool useCircle = false;
    int n = 0;
    qreal radius;
    if (c->containerType == Branch && hasFloatingBranchesLayout()) {
        useCircle = true;
        radius = 190;
        n = branchCount();
    } else if (c->containerType == Image && hasFloatingImagesLayout()) {
        useCircle = true;
        radius = 100;
        n = imageCount();
    }

    if (useCircle) {
        // Mapcenter, suggest to put image or mainbranch on circle around center
        qreal a = -M_PI_4 + M_PI_2 * n + (M_PI_4 / 2) * (n / 4 % 4);
        return QPointF (radius * cos(a), radius * sin(a));
    }

    QRectF r = headingContainer->rect();

    switch (orientation) {
        case LeftOfParent:
            return QPointF(r.left() - c->rect().width(), r.center().y());
            break;
        case RightOfParent:
            return QPointF(r.right(), r.center().y());
            break;
        default:
            return QPointF(r.left(), r.center().y());
            break;
    }
}

QPointF BranchContainer::getPositionHintRelink(Container *c, int d_pos, const QPointF &p_scene) // FIXME-1 not working correctly with multiple selected branches
{
    QPointF hint;

    QRectF r;

    if (hasFloatingBranchesLayout()) {
        // Floating layout, position on circle around center of myself
        r = headingContainer->rect();
        qreal radius = Geometry::distance(r.center(), r.topLeft()) + 20;

        QPointF center = mapToScene(r.center());
        qreal a = getAngle(p_scene - center);
        hint = center + QPointF (radius * cos(a), - radius * sin(a));
    } else {
        // Regular layout
        if (branchesContainer)
            r = branchesContainer->rect();  // FIXME-2 getPositionHintRelink: check rotation: is rect still correct or better mapped bbox/rect?
        qreal y;
        if (d_pos == 0)
            y = r.bottom();
        else
            y = r.bottom() - d_pos * c->rect().height();

        switch (orientation) {
            case LeftOfParent:
                hint = QPointF(-linkWidth + r.left() - c->rect().width(), y);
                break;
            default:
                hint = QPointF( linkWidth + r.right(), y);
                break;
        }
        hint = headingContainer->mapToScene(hint);
    }

    return hint;
}

QPointF BranchContainer::downLinkPos()
{
    return downLinkPos(orientation);
}

QPointF BranchContainer::downLinkPos(const Orientation &orientationChild)
{
    if (frameType(true) != FrameContainer::NoFrame) {
        if (!parentBranchContainer())
            // Framed MapCenter: Use center of frame
            return ornamentsContainer->mapToScene(
                    ornamentsContainer->center());
        else {
            // Framed branch: Use left or right edge
            switch (orientationChild) {
                case RightOfParent:
                    return ornamentsContainer->mapToScene(
                            ornamentsContainer->rightCenter());
                case LeftOfParent:
                    return ornamentsContainer->mapToScene(
                            ornamentsContainer->leftCenter());
                default:    // Shouldn't happen...
                    return ornamentsContainer->mapToScene(
                            ornamentsContainer->bottomLeft());
            }
        }
    }

    // No frame, return bottom left/right corner
    switch (orientationChild) {
        case RightOfParent:
            return ornamentsContainer->mapToScene(
                    ornamentsContainer->bottomRight());
        case LeftOfParent:
            return ornamentsContainer->mapToScene(
                    ornamentsContainer->bottomLeft());
        default:
            return ornamentsContainer->mapToScene(
                    ornamentsContainer->bottomRight());
    }
}

QPointF BranchContainer::upLinkPos(const Orientation &orientationChild)
{
    if (frameType(true) != FrameContainer::NoFrame ||
        frameType(false) != FrameContainer::NoFrame) {
        if (!parentBranchContainer())
            // Framed MapCenter: Use center of frame
            return ornamentsContainer->mapToScene(
                    ornamentsContainer->center());
        else {

            // Framed branch: Use left or right edge
            switch (orientationChild) {
                case RightOfParent:
                    return ornamentsContainer->mapToScene(
                            ornamentsContainer->leftCenter());
                case LeftOfParent:
                    return ornamentsContainer->mapToScene(
                            ornamentsContainer->rightCenter());
                default: // Shouldn't happen
                    return ornamentsContainer->mapToScene(
                            ornamentsContainer->bottomCenter());
            }
        }
    }

    // For branches without frames, return bottom left/right corner
    switch (orientationChild) {
        case RightOfParent:
            return ornamentsContainer->mapToScene(
                    ornamentsContainer->bottomLeft());
        case LeftOfParent:
            return ornamentsContainer->mapToScene(
                    ornamentsContainer->bottomRight());
        default:
            return ornamentsContainer->mapToScene(
                    ornamentsContainer->bottomLeft());
    }
}

void BranchContainer::updateUpLink()
{
    // Sets geometry and color of upLink and bottomline. 
    // Called from MapEditor e.g. during animation or 
    // from VymModel, when colors change

    // MapCenters still might have upLinks: The bottomLine is part of upLink!

    QPointF upLinkSelf_sp = upLinkPos(orientation);
    QPointF downLink_sp = downLinkPos();

    BranchContainer *pbc = nullptr;

    if (tmpLinkedParentContainer) {
        // I am temporarily linked to tmpLinkedParentContainer
        pbc = tmpLinkedParentContainer;
    } else if (originalParentBranchContainer)
        // I am moving with tmpParent, use original parent for link
        pbc = originalParentBranchContainer;
    else
        // Regular update, e.g. when linkStyle changes
        pbc = parentBranchContainer();

    BranchItem *tmpParentBI = nullptr;

    /*
    qDebug() << "BC::updateUpLink of " << info() << " tmpLinkedPC=" << tmpLinkedParentContainer 
             << " pbc=" << pbc
             << " orgPBC=" << originalParentBranchContainer;
     */

    if (pbc) {
        tmpParentBI = pbc->getBranchItem();
        QPointF upLinkParent_sp;

        upLinkParent_sp = pbc->downLinkPos();
        upLinkParent_sp = pbc->downLinkPos(orientation);

        QGraphicsItem* upLinkParent = upLink->parentItem();
        if (!upLinkParent) return;

        upLink->setUpLinkPosParent(
                upLinkParent->sceneTransform().inverted().map(upLinkParent_sp));
        upLink->setUpLinkPosSelf(
                upLinkParent->sceneTransform().inverted().map(upLinkSelf_sp));
        upLink->setDownLinkPos(
                upLinkParent->sceneTransform().inverted().map(downLink_sp));
    } else {
        // I am a MapCenter without parent. Add LinkObj to my own LinkContainer,
        // so that at least positions are updated and bottomLine can be drawn
        linkContainer->addLink(upLink);

        upLink->setLinkStyle(LinkObj::NoLink);

        upLink->setUpLinkPosSelf(
                linkContainer->sceneTransform().inverted().map(upLinkSelf_sp));
        upLink->setDownLinkPos(
                linkContainer->sceneTransform().inverted().map(downLink_sp));
    }


    // Color of link (depends on current parent)
    if (upLink->getLinkColorHint() == LinkObj::HeadingColor)
        upLink->setLinkColor(headingContainer->getColor());
    else {
        if (branchItem)
            upLink->setLinkColor(branchItem->mapDesign()->defaultLinkColor());
    }

    // Style of link
    if (tmpParentBI) {
        if (pbc && pbc->branchesContainerLayout == List)
            upLink->setLinkStyle(LinkObj::NoLink);
        else
            upLink->setLinkStyle(tmpParentBI->mapDesign()->linkStyle( 1 + tmpParentBI->depth()));
    }

    // Create/delete bottomline, depends on frame and (List-)Layout
    if (frameType(true) != FrameContainer::NoFrame &&
            upLink->hasBottomLine())
            upLink->deleteBottomLine();
    else {
        if (!upLink->hasBottomLine() && containerType != TmpParent)
            upLink->createBottomLine();
    }
    // Finally geometry
    upLink->updateLinkGeometry();
}

void BranchContainer::setLayout(const Layout &l)
{
    if (containerType != Branch && containerType != TmpParent)
        qWarning() << "BranchContainer::setLayout (...) called for non-branch: " << info();
    Container::setLayout(l);
}

void BranchContainer::setImagesContainerLayout(const Layout &ltype)
{
    if (imagesContainerLayout == ltype)
        return;

    imagesContainerLayout = ltype;

    if (imagesContainer)
        imagesContainer->setLayout(imagesContainerLayout);

    updateChildrenStructure();
}

Container::Layout BranchContainer::getImagesContainerLayout()
{
    return imagesContainerLayout;
}

void BranchContainer::setBranchesContainerLayout(const Layout &layoutNew)
{
    branchesContainerLayout = layoutNew;

    if (branchesContainer)
        branchesContainer->setLayout(branchesContainerLayout);
    updateChildrenStructure();
}

Container::Layout BranchContainer::getBranchesContainerLayout()
{
    return branchesContainerLayout;
}

void BranchContainer::setBranchesContainerVerticalAlignment(const VerticalAlignment &a)
{
    branchesContainerVerticalAlignment = a;
    if (branchesContainer)
        branchesContainer->setVerticalAlignment(branchesContainerVerticalAlignment);
}

void BranchContainer::setBranchesContainerBrush(const QBrush &b)
{
    branchesContainerBrush = b;
    if (branchesContainer)
        branchesContainer->setBrush(branchesContainerBrush);
}

QRectF BranchContainer::headingRect()
{
    // Returns scene coordinates of bounding rectanble
    return headingContainer->mapToScene(headingContainer->rect()).boundingRect();
}

void BranchContainer::setRotationHeading(const int &a)
{
    rotationHeadingInt = a;
    updateChildrenStructure();  // FIXME-2 or better do this in updateStyles()?
    //headingContainer->setScale(f + a * 1.1);      // FIXME-2 what about scaling?? Which transformCenter?
}

int BranchContainer::rotationHeading()
{
    return qRound(rotationHeadingInt);
}

int BranchContainer::rotationHeadingInScene()
{
    qreal r = rotationHeadingInt + rotationSubtreeInt;

    BranchContainer *pbc = parentBranchContainer();
    while (pbc) {
        r += pbc->rotationSubtree();
        pbc = pbc->parentBranchContainer();
    }

    return qRound(r);
}

void BranchContainer::setRotationSubtree(const int &a)
{
    rotationSubtreeInt = a;
    updateChildrenStructure();  // FIXME-2 or better do this in updateStyles()?
}

int BranchContainer::rotationSubtree()
{
    return qRound(rotationSubtreeInt);
}

QUuid BranchContainer::findFlagByPos(const QPointF &p)
{
    QUuid uid;

    if (systemFlagRowContainer) {
        uid = systemFlagRowContainer->findFlagByPos(p);
        if (!uid.isNull())
            return uid;
    }

    if (standardFlagRowContainer)
        uid = standardFlagRowContainer->findFlagByPos(p);

    return uid;
}

bool BranchContainer::isInClickBox(const QPointF &p)
{
    return ornamentsContainer->rect().contains(ornamentsContainer->mapFromScene(p));
}

QRectF BranchContainer::getBBoxURLFlag()
{
    Flag *f = systemFlagsMaster->findFlagByName("system-url");
    if (f) {
        QUuid u = f->getUuid();
        FlagContainer *fc = systemFlagRowContainer->findFlagContainerByUid(u);
        if (fc)
            return fc->mapRectToScene(fc->rect());
    }
    return QRectF();
}

void BranchContainer::select()
{
    SelectableContainer::select(
	    ornamentsContainer,
	    branchItem->mapDesign()->selectionPen(),
	    branchItem->mapDesign()->selectionBrush());
}

bool BranchContainer::frameAutoDesign(const bool &useInnerFrame)
{
    if (useInnerFrame)
        return autoDesignInnerFrame;
    else
        return autoDesignOuterFrame;
}

void BranchContainer::setFrameAutoDesign(const bool &useInnerFrame, const bool &b)
{
    if (useInnerFrame)
        autoDesignInnerFrame = b;
    else
        autoDesignOuterFrame = b;
}

FrameContainer::FrameType BranchContainer::frameType(const bool &useInnerFrame)
{
    if (useInnerFrame) {
        if (innerFrame)
            return innerFrame->frameType();
        else
            return FrameContainer::NoFrame;
    }

    if (outerFrame)
        return outerFrame->frameType();
    else
        return FrameContainer::NoFrame;
}

QString BranchContainer::frameTypeString(const bool &useInnerFrame)
{
    if (useInnerFrame) {
        if (innerFrame)
            return innerFrame->frameTypeString();
    } else
        if (outerFrame)
            return outerFrame->frameTypeString();

    return "NoFrame";
}

void BranchContainer::setFrameType(const bool &useInnerFrame, const FrameContainer::FrameType &ftype)
{
    if (useInnerFrame) {
        // Inner frame around ornamentsContainer
        if (ftype == FrameContainer::NoFrame) {
            if (innerFrame) {
                innerContainer->addContainer(ornamentsContainer, Z_ORNAMENTS);
                ornamentsContainer->setRotation(innerFrame->rotation());
                delete innerFrame;
                innerFrame = nullptr;
            }
        } else {
            if (!innerFrame) {
                innerFrame = new FrameContainer;
                innerFrame->setUsage(FrameContainer::InnerFrame);
                innerFrame->addContainer(ornamentsContainer, Z_ORNAMENTS);
                innerFrame->setRotation(ornamentsContainer->rotation());
                innerContainer->addContainer(innerFrame, Z_INNER_FRAME);
                ornamentsContainer->setRotation(0);
            }
            innerFrame->setFrameType(ftype);
        }
    } else {
        // Outer frame around whole branchContainer including children
        if (ftype == FrameContainer::NoFrame) {
            // Remove outerFrame
            if (outerFrame) {
                int a = rotationSubtree();
                Container *c;
                if (outerContainer)
                    c = outerContainer;
                else
                    c = innerContainer;
                addContainer(c);
                delete outerFrame;
                outerFrame = nullptr;
                setRotationSubtree(a);
            }
        } else {
            // Set outerFrame
            if (!outerFrame) {
                int a = rotationSubtree();
                outerFrame = new FrameContainer;
                outerFrame->setUsage(FrameContainer::OuterFrame);
                Container *c;
                if (outerContainer)
                    c = outerContainer;
                else
                    c = innerContainer;
                outerFrame->addContainer(c);
                addContainer(outerFrame, Z_OUTER_FRAME);
            }
            outerFrame->setFrameType(ftype);
        }
    }
    updateChildrenStructure();
}

void BranchContainer::setFrameType(const bool &useInnerFrame, const QString &s)
{
    FrameContainer::FrameType ftype = FrameContainer::frameTypeFromString(s);
    setFrameType(useInnerFrame, ftype);
}

int BranchContainer::framePadding(const bool &useInnerFrame)
{
    if (useInnerFrame) {
        if (innerFrame)
            return innerFrame->framePadding();
    } else {
        if (outerFrame)
            return outerFrame->framePadding();
    }

    return 0;
}

void BranchContainer::setFramePadding(const bool &useInnerFrame, const int &p)
{
    if (useInnerFrame) {
        if (innerFrame)
            innerFrame->setFramePadding(p);
    } else {
        if (outerFrame)
            outerFrame->setFramePadding(p);
    }
}

qreal BranchContainer::frameTotalPadding(const bool &useInnerFrame) // padding +  pen width + xsize (e.g. cloud)
{
    if (useInnerFrame) {
        if (innerFrame)
            return innerFrame->frameTotalPadding();
    } else {
        if (outerFrame)
            return outerFrame->frameTotalPadding();
    }

    return 0;
}

qreal BranchContainer::frameXPadding(const bool &useInnerFrame)
{
    if (useInnerFrame) {
        if (innerFrame)
            return innerFrame->frameXPadding();
    } else {
        if (outerFrame)
            return outerFrame->frameXPadding();
    }

    return 0;
}

int BranchContainer::framePenWidth(const bool &useInnerFrame)
{
    if (useInnerFrame) {
        if (innerFrame)
            return innerFrame->framePenWidth();
    } else {
        if (outerFrame)
            return outerFrame->framePenWidth();
    }

    return 0;
}

void BranchContainer::setFramePenWidth(const bool &useInnerFrame, const int &w)
{
    if (useInnerFrame) {
       if (innerFrame)
           innerFrame->setFramePenWidth(w);
    } else {
        if (outerFrame)
           outerFrame->setFramePenWidth(w);
    }
}

QColor BranchContainer::framePenColor(const bool &useInnerFrame)
{
    if (useInnerFrame) {
        if (innerFrame)
            return innerFrame->framePenColor();
    } else {
        if (outerFrame)
            return outerFrame->framePenColor();
    }

    return QColor();
}

void BranchContainer::setFramePenColor(const bool &useInnerFrame, const QColor &c)
{
    if (useInnerFrame) {
        if (innerFrame)
            innerFrame->setFramePenColor(c);
    } else {
        if (outerFrame)
            outerFrame->setFramePenColor(c);
    }
}

QColor BranchContainer::frameBrushColor(const bool &useInnerFrame)
{
    if (useInnerFrame) {
        if (innerFrame)
            return innerFrame->frameBrushColor();
    } else {
        if (outerFrame)
            return outerFrame->frameBrushColor();
    }

    return QColor();
}

void BranchContainer::setFrameBrushColor(const bool &useInnerFrame, const QColor &c)
{
    if (useInnerFrame) {
        if (innerFrame)
            innerFrame->setFrameBrushColor(c);
    } else {
        if (outerFrame)
            outerFrame->setFrameBrushColor(c);
    }
}

QString BranchContainer::saveFrame()
{
    QString r;
    if (innerFrame && innerFrame->frameType() != FrameContainer::NoFrame)
        r = innerFrame->saveFrame();

    if (outerFrame && outerFrame->frameType() != FrameContainer::NoFrame)
        r += outerFrame->saveFrame();
    return r;
}

void BranchContainer::updateBranchesContainerLayout()
{
    // Set container layouts
    if (branchItem && branchesContainerAutoLayout)
        setBranchesContainerLayout(
            branchItem->mapDesign()->branchesContainerLayout(
                branchItem->depth()));
    else
        setBranchesContainerLayout(branchesContainerLayout);
}

void BranchContainer::updateStyles(const MapDesign::UpdateMode &updateMode)
{
    // Note: updateStyles() is never called for TmpParent!

    //qDebug() << "BC::updateStyles of " << info(); // FIXME-3 testing

    uint depth = branchItem->depth();
    MapDesign *md = branchItem->mapDesign();
    BranchContainer *pbc = parentBranchContainer();

    // Set heading color (might depend on parentBranch, so pass the branchItem)
    if (updateMode == MapDesign::CreatedByUser)
        md->updateBranchHeadingColor(updateMode, branchItem, depth);

    // bulletpoint color should match heading color
    if (bulletPointContainer)
        bulletPointContainer->setHeadingColor(headingContainer->getHeadingColor());

    // Set frame
    md->updateFrames(this, updateMode, depth); // FIXME-2  depth not really necessary here

    updateBranchesContainerLayout();

    if (imagesContainerAutoLayout)
        setImagesContainerLayout(
                md->imagesContainerLayout(depth));
    else
        setImagesContainerLayout(imagesContainerLayout);

    // FIXME-5 for testing we do some coloring and additional drawing
    /*
    if (containerType != TmpParent) {
        // BranchContainer
        //setPen(QPen(Qt::green));

        // OrnamentsContainer
        //ornamentsContainer->setPen(QPen(Qt::blue));
        //ornamentsContainer->setPen(Qt::NoPen);

        // InnerContainer
        //innerContainer->setPen(QPen(Qt::cyan));

        if (branchesContainer) branchesContainer->setPen(QColor(Qt::gray));

        // Background colors for floating content
        QColor col;
        if (branchesContainerLayout == FloatingBounded && depth > 0) {
            // Special layout: FloatingBounded
            col = QColor(Qt::gray);
            col.setAlpha(150);
            setBranchesContainerBrush(col);
        } else if (branchesContainerLayout == FloatingFree) {
            // Special layout: FloatingFree
            col = QColor(Qt::blue);
            col.setAlpha(120);
            setBrush(col);
        } else {
            // Don't paint other containers
            setBranchesContainerBrush(Qt::NoBrush);
            setBrush(Qt::NoBrush);
        }
    }   // Visualizations for testing
    */
}

void BranchContainer::updateVisuals()
{
    // Update heading
    if (branchItem)
        headingContainer->setHeading(branchItem->getHeadingText());

    // Update standard flags active in TreeItem
    QList<QUuid> TIactiveFlagUids = branchItem->activeFlagUids();
    if (TIactiveFlagUids.count() == 0) {
        if (standardFlagRowContainer) {
            delete standardFlagRowContainer;
            standardFlagRowContainer = nullptr;
        }
    } else {
        if (!standardFlagRowContainer) {
            standardFlagRowContainer = new FlagRowContainer;
            ornamentsContainer->addContainer(standardFlagRowContainer, Z_STANDARD_FLAGS);
        }
        standardFlagRowContainer->updateActiveFlagContainers(
            TIactiveFlagUids, standardFlagsMaster, userFlagsMaster);
    }

    // Add missing system flags active in TreeItem
    TIactiveFlagUids = branchItem->activeSystemFlagUids();
    if (TIactiveFlagUids.count() == 0) {
        if (systemFlagRowContainer) {
            delete systemFlagRowContainer;
            systemFlagRowContainer = nullptr;
        }
    } else {
        if (!systemFlagRowContainer) {
            systemFlagRowContainer = new FlagRowContainer;
            ornamentsContainer->addContainer(systemFlagRowContainer, Z_SYSTEM_FLAGS);
        }
        systemFlagRowContainer->updateActiveFlagContainers(
            TIactiveFlagUids, systemFlagsMaster);
    }
}

void BranchContainer::reposition()
{
    // Abreviation for depth
    uint depth;
    if (branchItem)
        depth = branchItem->depth();
    else
        // tmpParentContainer has no branchItem
        depth = 0;

    // Set orientation based on depth and if we are floating around or
    // in the process of being (temporary) relinked
    BranchContainer *pbc = parentBranchContainer();

    if (!pbc && containerType != TmpParent) {
        // I am a (not moving) mapCenter
        orientation = UndefinedOrientation;
    } else {
        if (containerType != TmpParent) {
            // "regular repositioning", not currently moved in MapEditor
            if (!pbc)
                orientation = UndefinedOrientation;
            else {
                /*
                qdbg() << ind() << "BC::repos pbc=" << pbc->info();
                qdbg() << ind() << "BC::repos pbc->orientation=" << pbc->orientation;
                */

                if (pbc->orientation == UndefinedOrientation) {
                    // Parent is tmpParentContainer or mapCenter
                    // use relative position to determine orientation

                    if (parentContainer()->hasFloatingLayout()) {
                        if (pos().x() >= 0)
                            orientation = RightOfParent;
                        else
                            orientation = LeftOfParent;
                        /* FIXME-5 remove comments
                        qdbg() << ind() << "BC: Setting neworient " << orientation << " in: " << info();
                        qdbg() << ind() << "    pc: " << parentContainer()->info();
                        */
                    } else {
                        // Special case: Mainbranch in horizontal or vertical layout
                        orientation = RightOfParent;
                    }
                } else {
                    // Set same orientation as parent
                    setOrientation(pbc->orientation);
                    //qdbg() << ind() << "BC: Setting parentorient " << orientation << " in: " << info();
                }
            }
        } // else:
        // The "else" here would be that I'm the tmpParentContainer, but
        // then my orientation is already set in MapEditor, so ignore here
    }

    // Settings depending on depth
    if (depth == 0)
    {
        // MapCenter or TmpParent?
        if (containerType != TmpParent) {
            setHorizontalDirection(LeftToRight);
            // FIXME-2 set in updateChildrenStructure: innerContainer->setHorizontalDirection(LeftToRight);
        }

        // FIXME-2 set in updateChildrenStructure: innerContainer->setLayout(BoundingFloats);
    } else {
        // Branch or mainbranch
        switch (orientation) {
            case LeftOfParent:
                setHorizontalDirection(RightToLeft);
                innerContainer->setHorizontalDirection(RightToLeft);
                setBranchesContainerVerticalAlignment(AlignedRight);
                break;
            case RightOfParent:
                setHorizontalDirection(LeftToRight);
                innerContainer->setHorizontalDirection(LeftToRight);
                setBranchesContainerVerticalAlignment(AlignedLeft);
                break;
            case UndefinedOrientation:
                qWarning() << "BC::reposition - UndefinedOrientation in " << info();
                break;
            default:
                qWarning() << "BC::reposition - Unknown orientation " << orientation << " in " << info();
                break;
        }
    }

    Container::reposition();

    // Update links of children
    if (branchesContainer && branchCount() > 0) {
        foreach (Container *c, branchesContainer->childContainers()) {
            BranchContainer *bc = (BranchContainer*) c;
            bc->updateUpLink();
        }
    }

    // Update my own bottomLine, in case I am a MapCenter 
    if (depth == 0)
        updateUpLink();

    // (tmpParentContainer has no branchItem!)
    if (branchItem) {
        XLinkObj *xlo;
        for (int i = 0; i < branchItem->xlinkCount(); ++i) {
            xlo = branchItem->getXLinkObjNum(i);
            if (xlo)
                xlo->updateXLink();
        }
    }

    // Update frames
    if (innerFrame)
        innerFrame->setFrameRect(ornamentsContainer->rect());

    if (outerFrame) {
        if (outerContainer) {
            outerFrame->setFrameRect(outerContainer->rect());
            outerFrame->setFramePos(outerContainer->pos());
        } else {
            outerFrame->setFrameRect(innerContainer->rect());
            outerFrame->setFramePos(innerContainer->pos());
        }
    }
}
