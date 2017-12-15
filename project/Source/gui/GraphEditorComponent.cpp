/*
    GraphEditorComponent.cpp - This file is part of Element
    Copyright (C) 2016  Kushview, LLC.  All rights reserved.
*/

#include "ElementApp.h"
#include "controllers/GraphController.h"
#include "gui/GuiCommon.h"
#include "gui/ContentComponent.h"
#include "gui/ContextMenus.h"
#include "gui/PluginWindow.h"
#include "engine/SubGraphProcessor.h"
#include "session/PluginManager.h"
#include "session/Node.h"

#include "gui/GraphEditorComponent.h"

namespace Element {
class PinComponent   : public Component,
                       public SettableTooltipClient
{
public:
    PinComponent (const Node& graph_, const Node& node_,
                  const uint32 filterID_,
                  const uint32 index_, const bool isInput_,
                  const PortType type_, const bool verticle_)
        : filterID (filterID_),
          port (index_),
          type (type_),
          isInput (isInput_),
          graph (graph_), node (node_),
          verticle (verticle_)
    {
        if (const GraphNodePtr obj = node.getGraphNode())
        {
            const Port p (node.getPort ((int) port));
            String tip = p.getName();
            
            if (tip.isEmpty()) {
                tip = (isInput ? "Input " : "Output ") + String (index_ + 1);
            }

            setTooltip (tip);
        }

        setSize (16, 16);
    }

    void paint (Graphics& g)
    {
        const float w = (float) getWidth();
        const float h = (float) getHeight();

        Path p;
        if (verticle)
        {
            p.addEllipse (w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f);
            p.addRectangle (w * 0.4f,
                            isInput ? (0.5f * h) : 0.0f,
                            w * 0.2f,
                            h * 0.5f);
        }
        else
        {
            p.addEllipse (w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f);
            p.addRectangle (isInput ? (0.5f * w) : 0.0f,
                            h * 0.4f,
                            w * 0.5f,
                            h * 0.2f);

        }
        
        g.setColour (getColor());
        g.fillPath (p);
    }

    Colour getColor() const
    {
        switch (this->type)
        {
            case PortType::Audio:   return Colours::lightgreen; break;
            case PortType::Control: return Colours::lightblue;  break;
            default: break;
        }
        
        return Colours::orange;
    }

    void mouseDown (const MouseEvent& e)
    {
        getGraphPanel()->beginConnectorDrag (isInput ? 0 : filterID,
                                             port,
                                             isInput ? filterID : 0,
                                             port,
                                             e);
    }

    void mouseDrag (const MouseEvent& e)
    {
        getGraphPanel()->dragConnector (e);
    }

    void mouseUp (const MouseEvent& e)
    {
        getGraphPanel()->endDraggingConnector (e);
    }

    const uint32    filterID;
    const uint32    port;
    const PortType  type;
    const bool      isInput;

private:
    Node graph, node;
    bool verticle;
    GraphEditorComponent* getGraphPanel() const noexcept
    {
        return findParentComponentOfClass<GraphEditorComponent>();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PinComponent)
};

class FilterComponent    : public Component,
                           public ButtonListener
{
public:
    FilterComponent (const Node& graph_, const Node& node_, const bool vertical_)
        : filterID (node_.getNodeId()),
          graph (graph_), 
          node (node_),
          numInputs (0), 
          numOutputs (0),
          pinSize (16), 
          font (13.0f, Font::bold),
          numIns (0), 
          numOuts (0), 
          vertical (vertical_)
    {
        shadow.setShadowProperties (DropShadow (Colours::black.withAlpha (0.5f), 3, Point<int> (0, 1)));
        setComponentEffect (&shadow);
        
        addAndMakeVisible (ioButton);
        ioButton.setButtonText ("IO");
        ioButton.addListener (this);

        setSize (vertical ? 150 : 170, 60);
    }

    void buttonClicked (Button*) override 
    {
        if (ioButton.getToggleState())
        {
            ioButton.setToggleState (false, dontSendNotification);
            ioBox.clear();
        }
        else if (! ioButton.getToggleState())
        {
            auto* component = new Component();
            component->setSize (200, 100);

            auto& box = CallOutBox::launchAsynchronously (
                component, ioButton.getScreenBounds(), 0);
            ioBox.setNonOwned (&box);
        }
    }

    void deleteAllPins()
    {
        for (int i = getNumChildComponents(); --i >= 0;)
            if (auto * c = dynamic_cast<PinComponent*> (getChildComponent(i)))
                delete c;
    }

    ~FilterComponent()
    {
        deleteAllPins();
    }

    void mouseDown (const MouseEvent& e) override
    {
        originalPos = localPointToGlobal (Point<int>());
        toFront (true);

        if (e.mods.isPopupMenu())
        {
            NodePopupMenu menu (node);
            menu.addSeparator();
            menu.addProgramsMenu();
            menu.addPresetsMenu();
            if (auto* message = menu.showAndCreateMessage())
                ViewHelpers::postMessageFor (this, message);
        }
    }

    void updatePosition()
    {
        node.getRelativePosition (relativeX, relativeY);
        vertical ? setCentreRelative (relativeX, relativeY)
                 : setCentreRelative (relativeY, relativeX);
        getGraphPanel()->updateConnectorComponents();
    }
    
    void mouseDrag (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;
        
        Point<int> pos (originalPos + Point<int> (e.getDistanceFromDragStartX(), e.getDistanceFromDragStartY()));
        
        if (getParentComponent() != nullptr)
            pos = getParentComponent()->getLocalPoint (nullptr, pos);
        
        if (vertical)
        {
            node.setRelativePosition ((pos.getX() + getWidth() / 2) / (double) getParentWidth(),
                                      (pos.getY() + getHeight() / 2) / (double) getParentHeight());
        }
        else
        {
            node.setRelativePosition ((pos.getY() + getHeight() / 2) / (double) getParentHeight(),
                                      (pos.getX() + getWidth() / 2) / (double) getParentWidth());
        }
        
        updatePosition();
    }

    void makeEditorActive()
    {
        if (node.isGraph())
        {
            // TODO: this can cause a crash, do it async
            if (auto* cc = ViewHelpers::findContentComponent (this))
                cc->setCurrentNode (node);
        }
        else if (node.isValid())
        {
            ViewHelpers::presentPluginWindow (this, node);
        }
    }
    
    void mouseUp (const MouseEvent& e) override
    {
        if (e.mouseWasClicked() && e.getNumberOfClicks() == 2)
        {
            makeEditorActive();
        }
        else if (! e.mouseWasClicked())
        {
            // FIXME:
            // graph.sendChangeMessage();
        }
    }

    bool hitTest (int x, int y) override
    {
        for (int i = getNumChildComponents(); --i >= 0;)
            if (getChildComponent(i)->getBounds().contains (x, y))
                return true;

        return vertical ? x >= 3 && x < getWidth() - 6 && y >= pinSize && y < getHeight() - pinSize
                        : y >= 3 && y < getHeight() - 6 && x >= pinSize && x < getWidth() - pinSize;
    }

    Rectangle<int> getBoxRectangle() const
    {
        if (vertical)
        {
            const int x = 4;
            const int y = pinSize;
            const int w = getWidth() - x * 2;
            const int h = getHeight() - pinSize * 2;
            
            return Rectangle<int> (x, y, w, h);
        }

        const int x = pinSize;
        const int y = 4;
        const int w = getWidth() - pinSize * 2;
        const int h = getHeight() - y * 2;
        
        return { x, y, w, h };
    }
    
    void paint (Graphics& g)
    {
        g.setColour (Colours::lightgrey);
        
        const auto box (getBoxRectangle());
        g.fillRect (box);

        g.setColour (Colours::black);
        g.setFont (font);
        g.drawFittedText (getName(), getLocalBounds().reduced (4, 2), Justification::centred, 2);

        g.setColour (Colours::grey);
        g.drawRect (box);
    }

    void resized()
    {
        auto r (getBoxRectangle());
        ioButton.setBounds (r.removeFromTop(20).removeFromRight (16));
        int indexIn = 0, indexOut = 0;
        if (vertical)
        {
            for (int i = 0; i < getNumChildComponents(); ++i)
            {
                if (PinComponent* const pc = dynamic_cast <PinComponent*> (getChildComponent(i)))
                {
                    const int total = pc->isInput ? numIns : numOuts;
                    const int index = pc->isInput ? indexIn++ : indexOut++;
                    pc->setBounds (proportionOfWidth ((1 + index) / (total + 1.0f)) - pinSize / 2,
                                   pc->isInput ? 0 : (getHeight() - pinSize),
                                   pinSize, pinSize);
                }
            }
        }
        else
        {
            for (int i = 0; i < getNumChildComponents(); ++i)
            {
                if (PinComponent* const pc = dynamic_cast <PinComponent*> (getChildComponent(i)))
                {
                    const int total = pc->isInput ? numIns : numOuts;
                    const int index = pc->isInput ? indexIn++ : indexOut++;
                    pc->setBounds (pc->isInput ? 0 : (getWidth() - pinSize),
                                   proportionOfHeight ((1 + index) / (total + 1.0f)) - pinSize / 2,
                                   pinSize, pinSize);
                }
            }
        }
    }

    void getPinPos (const int index, const bool isInput, float& x, float& y)
    {
        for (int i = 0; i < getNumChildComponents(); ++i)
        {
            if (PinComponent* const pc = dynamic_cast <PinComponent*> (getChildComponent(i)))
            {
                if (pc->port == index && isInput == pc->isInput)
                {
                    x = getX() + pc->getX() + pc->getWidth() * 0.5f;
                    y = getY() + pc->getY() + pc->getHeight() * 0.5f;
                    break;
                }
            }
        }
    }

    void update()
    {
        vertical = getGraphPanel()->isLayoutVertical();
        
        if (! node.getValueTree().getParent().hasType (Tags::nodes))
        {
            delete this;
            return;
        }

        numIns = numOuts = 0;
        const auto numPorts = node.getPortsValueTree().getNumChildren();
        for (int i = 0; i < numPorts; ++i)
        {
            const Port port (node.getPort (i));
            if (PortType::Control == port.getType())
                continue;
            
            if (port.isInput())
                ++numIns;
            else
                ++numOuts;
        }

        int w = 100;
        int h = 60;

        w = vertical ? jmax (w, (jmax (numIns, numOuts) + 1) * 20) : 120;
        h = vertical ? h : jmax (h, (jmax (numIns, numOuts) + 1) * 20);
        
        const int textWidth = font.getStringWidth (node.getName());
        w = jmax (w, 16 + jmin (textWidth, 300));
        
        if (vertical && textWidth > 300)
            h = 100;
        
        setSize (w, h);

        setName (node.getName());
        updatePosition();

        if (numIns != numInputs || numOuts != numOutputs)
        {
            numInputs = numIns;
            numOutputs = numOuts;

            deleteAllPins();

            for (uint32 i = 0; i < numPorts; ++i)
            {
                const Port port (node.getPort (i));
                const PortType t (port.getType());
                if (t == PortType::Control)
                    continue;
                
                const bool isInput (port.isInput());
                addAndMakeVisible (new PinComponent (graph, node, filterID, i, isInput, t, vertical));
            }

            resized();
        }
    }
    
    const uint32 filterID;

private:
    Node graph;
    Node node;
    int numInputs, numOutputs;
    double relativeX = 0.5f;
    double relativeY = 0.5f;
    int pinSize;
    Point<int> originalPos;
    Font font;
    int numIns, numOuts;
    bool vertical = true;
    
    SettingButton ioButton;
    OptionalScopedPointer<CallOutBox> ioBox;

    DropShadowEffect shadow;
    ScopedPointer<Component> embedded;

    GraphEditorComponent* getGraphPanel() const noexcept
    {
        return findParentComponentOfClass<GraphEditorComponent>();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterComponent);
};

class ConnectorComponent   : public Component,
                             public SettableTooltipClient
{
public:
    ConnectorComponent (const Node& graph_)
        : sourceFilterID (0), destFilterID (0),
          sourceFilterChannel (0), destFilterChannel (0),
          graph (graph_),
          lastInputX (0), lastInputY (0),
          lastOutputX (0), lastOutputY (0)
    {
        setAlwaysOnTop (true);
    }

    ~ConnectorComponent() { }

    bool isDragging() const { return dragging; }

    void setInput (const uint32 sourceFilterID_, const int sourceFilterChannel_)
    {
        if (sourceFilterID != sourceFilterID_ || sourceFilterChannel != sourceFilterChannel_)
        {
            sourceFilterID = sourceFilterID_;
            sourceFilterChannel = sourceFilterChannel_;
            update();
        }
    }

    void setOutput (const uint32 destFilterID_, const int destFilterChannel_)
    {
        if (destFilterID != destFilterID_ || destFilterChannel != destFilterChannel_)
        {
            destFilterID = destFilterID_;
            destFilterChannel = destFilterChannel_;
            update();
        }
    }

    void dragStart (int x, int y)
    {
        lastInputX = (float) x;
        lastInputY = (float) y;
        resizeToFit();
    }

    void dragEnd (int x, int y)
    {
        lastOutputX = (float) x;
        lastOutputY = (float) y;
        resizeToFit();
    }

    void update()
    {
        float x1, y1, x2, y2;
        getPoints (x1, y1, x2, y2);

        if (lastInputX != x1
             || lastInputY != y1
             || lastOutputX != x2
             || lastOutputY != y2)
        {
            resizeToFit();
        }
    }

    void resizeToFit()
    {
        float x1, y1, x2, y2;
        getPoints (x1, y1, x2, y2);

        const Rectangle<int> newBounds ((int) jmin (x1, x2) - 4,
                                        (int) jmin (y1, y2) - 4,
                                        (int) fabsf (x1 - x2) + 8,
                                        (int) fabsf (y1 - y2) + 8);

        setBounds (newBounds);
        repaint();
    }

    void getPoints (float& x1, float& y1, float& x2, float& y2) const
    {
        x1 = lastInputX;
        y1 = lastInputY;
        x2 = lastOutputX;
        y2 = lastOutputY;

        if (GraphEditorComponent* const hostPanel = getGraphPanel())
        {
            if (FilterComponent* srcFilterComp = hostPanel->getComponentForFilter (sourceFilterID))
                srcFilterComp->getPinPos (sourceFilterChannel, false, x1, y1);

            if (FilterComponent* dstFilterComp = hostPanel->getComponentForFilter (destFilterID))
                dstFilterComp->getPinPos (destFilterChannel, true, x2, y2);
        }
    }

    void paint (Graphics& g)
    {
        g.setColour (Colours::black.brighter());
        g.fillPath (linePath);
    }

    bool hitTest (int x, int y)
    {
        if (hitPath.contains ((float) x, (float) y))
        {
            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds (x, y, distanceFromStart, distanceFromEnd);

            // avoid clicking the connector when over a pin
            return distanceFromStart > 7.0 && distanceFromEnd > 7.0;
        }

        return false;
    }

    void mouseDown (const MouseEvent&)
    {
        dragging = false;
    }

    void mouseDrag (const MouseEvent& e)
    {
        if ((! dragging) && ! e.mouseWasClicked())
        {
            dragging = true;
            
            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds (e.x, e.y, distanceFromStart, distanceFromEnd);
            const bool isNearerSource = (distanceFromStart < distanceFromEnd);
            
            if (graph.isRootGraph())
            {
                ViewHelpers::postMessageFor (this, new RemoveConnectionMessage (
                    sourceFilterID, (uint32)sourceFilterChannel,
                    destFilterID, (uint32)destFilterChannel));
                
                // start draging before removing connection so
                // the wire doesn't get deleted before hand
                getGraphPanel()->beginConnectorDrag (isNearerSource ? 0 : sourceFilterID, sourceFilterChannel,
                                                     isNearerSource ? destFilterID : 0,
                                                     destFilterChannel,
                                                     e);
            }
            else if (graph.isGraph())
            {
                const Node node (graph.getNodeById (isNearerSource ? sourceFilterID : destFilterID));
                getGraphPanel()->data.removeListener (getGraphPanel());
                
                if (GraphNodePtr ptr = graph.getGraphNode())
                    if (auto* proc = dynamic_cast<SubGraphProcessor*> (ptr->getAudioProcessor()))
                        proc->getController().removeConnection (sourceFilterID, (uint32)sourceFilterChannel,
                                                                destFilterID, (uint32)destFilterChannel);
                
                getGraphPanel()->data.addListener (getGraphPanel());
                
                // start draging before removing connection so
                // the wire doesn't get deleted before hand
                getGraphPanel()->beginConnectorDrag (isNearerSource ? 0 : sourceFilterID,
                                                     sourceFilterChannel,
                                                     isNearerSource ? destFilterID : 0,
                                                     destFilterChannel,
                                                     e);
                
                getGraphPanel()->updateConnectorComponents();
            }
        }
        else if (dragging)
        {
            getGraphPanel()->dragConnector (e);
        }
    }

    void mouseUp (const MouseEvent& e)
    {
        if (dragging)
            getGraphPanel()->endDraggingConnector (e);
    }

    void resized()
    {
        float x1, y1, x2, y2;
        getPoints (x1, y1, x2, y2);

        lastInputX = x1;
        lastInputY = y1;
        lastOutputX = x2;
        lastOutputY = y2;

        x1 -= getX();
        y1 -= getY();
        x2 -= getX();
        y2 -= getY();

        linePath.clear();
        linePath.startNewSubPath (x1, y1);
        const bool vertical = getGraphPanel()->isLayoutVertical();
        
        if (vertical)
        {
            linePath.cubicTo (x1, y1 + (y2 - y1) * 0.33f,
                              x2, y1 + (y2 - y1) * 0.66f,
                              x2, y2);
        }
        else
        {
            linePath.cubicTo (x1 + (x2 - x1) * 0.33f, y1,
                              x1 + (x2 - x1) * 0.66f, y2,
                              x2, y2);
        }
        
        PathStrokeType wideStroke (8.0f);
        wideStroke.createStrokedPath (hitPath, linePath);

        PathStrokeType stroke (2.5f);
        stroke.createStrokedPath (linePath, linePath);

        const bool showArrow = false;
        
        if (showArrow)
        {
            const float arrowW = 5.0f;
            const float arrowL = 4.0f;

            Path arrow;
            arrow.addTriangle (-arrowL, arrowW,
                               -arrowL, -arrowW,
                               arrowL, 0.0f);

            arrow.applyTransform (AffineTransform::identity
                                    .rotated (float_Pi * 0.5f - (float) atan2 (x2 - x1, y2 - y1))
                                    .translated ((x1 + x2) * 0.5f,
                                                 (y1 + y2) * 0.5f));

            linePath.addPath (arrow);
        }
        linePath.setUsingNonZeroWinding (true);
    }

    uint32 sourceFilterID, destFilterID;
    int sourceFilterChannel, destFilterChannel;

private:
    Node graph;
    float lastInputX, lastInputY, lastOutputX, lastOutputY;
    Path linePath, hitPath;
    bool dragging;

    GraphEditorComponent* getGraphPanel() const noexcept
    {
        return findParentComponentOfClass<GraphEditorComponent>();
    }

    void getDistancesFromEnds (int x, int y, double& distanceFromStart, double& distanceFromEnd) const
    {
        float x1, y1, x2, y2;
        getPoints (x1, y1, x2, y2);

        distanceFromStart = juce_hypot (x - (x1 - getX()), y - (y1 - getY()));
        distanceFromEnd = juce_hypot (x - (x2 - getX()), y - (y2 - getY()));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConnectorComponent)
};

GraphEditorComponent::GraphEditorComponent()
{
    setOpaque (true);
    data.addListener (this);
}

GraphEditorComponent::~GraphEditorComponent()
{
    data.removeListener (this);
    draggingConnector = nullptr;
    deleteAllChildren();
}

void GraphEditorComponent::setNode (const Node& n)
{
    bool isGraph = n.isGraph();
    bool isValid = n.isValid();
    graph = isValid && isGraph ? n : Node(Tags::graph);
    
    data.removeListener (this);
    data = graph.getValueTree();
    
    #if 0
    auto d2 = data.createCopy();
    Node::sanitizeProperties(d2, true);
    DBG(d2.toXmlString());
    #endif
    
    verticalLayout = graph.getProperty ("vertical", true);
    draggingConnector = nullptr;
    deleteAllChildren();
    updateComponents();

    data.addListener (this);
}

void GraphEditorComponent::setVerticalLayout (const bool isVertical)
{
    if (verticalLayout == isVertical)
        return;
    verticalLayout = isVertical;
    
    if (graph.isValid() && graph.isGraph())
        graph.setProperty ("vertical", verticalLayout);
    
    draggingConnector = nullptr;
    deleteAllChildren();
    updateComponents();
}

void GraphEditorComponent::paint (Graphics& g)
{
    g.fillAll (LookAndFeel::contentBackgroundColor);
}

void GraphEditorComponent::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        PluginsPopupMenu menu (this);
        if (graph.isGraph())
        {
            menu.addSectionHeader ("Graph I/O");
            menu.addItem (1, "Audio Inputs",    true, graph.hasAudioInputNode());
            menu.addItem (2, "Audio Outputs",   true, graph.hasAudioOutputNode());
            menu.addItem (3, "MIDI Input",      true, graph.hasMidiInputNode());
            menu.addItem (4, "MIDI Output",     true, graph.hasMidiOutputNode());
        }
        
        menu.addSeparator();
        menu.addItem (5, "Change orientation...");
        menu.addSeparator();
        
        menu.addSectionHeader ("Plugins");
        menu.addPluginItems();
        const int result = menu.show();
        
        if (menu.isPluginResultCode (result))
        {
            bool verified = false;
            if (const auto* desc = menu.getPluginDescription (result, verified))
                ViewHelpers::postMessageFor (this, new AddPluginMessage (graph, *desc));
        }
        else
        {
            PluginDescription desc;
            desc.pluginFormatName = "Internal";
            bool hasRequestedType = false;
            bool failure = false;
            ValueTree node;
            
            switch (result)
            {
                case 1:
                    desc.fileOrIdentifier = "audio.input";
                    hasRequestedType = graph.hasAudioInputNode();
                    break;
                case 2:
                    desc.fileOrIdentifier = "audio.output";
                    hasRequestedType = graph.hasAudioOutputNode();
                    break;
                case 3:
                    desc.fileOrIdentifier = "midi.input";
                    hasRequestedType = graph.hasMidiInputNode();
                    break;
                case 4:
                    desc.fileOrIdentifier = "midi.output";
                    hasRequestedType = graph.hasMidiOutputNode();
                    break;
                case 5:
                    setVerticalLayout (! isLayoutVertical());
                    return;
                    break;
                default:
                    failure = true;
                    break;
            }
            
            if (failure)
            {
                DBG("[EL] unkown menu result: " << result);
            }
            else if (hasRequestedType)
            {
                const ValueTree requestedNode = graph.getNodesValueTree()
                  .getChildWithProperty (Tags::identifier, desc.fileOrIdentifier);
                const Node model (requestedNode, false);
                ViewHelpers::postMessageFor (this, new RemoveNodeMessage (model));
            }
            else
            {
                DBG("X: " << e.position.getX() / (float)getWidth() <<
                   " Y: " << e.position.getY() / (float)getHeight());
                ViewHelpers::postMessageFor (this, new AddPluginMessage (graph, desc));
            }
        }
    }
}

void GraphEditorComponent::createNewPlugin (const PluginDescription* desc, int x, int y)
{
    DBG("[EL] GraphEditorComponent::createNewPlugin(...)");
}

FilterComponent* GraphEditorComponent::getComponentForFilter (const uint32 filterID) const
{
    for (int i = getNumChildComponents(); --i >= 0;)
    {
        if (FilterComponent* const fc = dynamic_cast <FilterComponent*> (getChildComponent (i)))
            if (fc->filterID == filterID)
                return fc;
    }

    return nullptr;
}

ConnectorComponent* GraphEditorComponent::getComponentForConnection (const Arc& arc) const
{
    for (int i = getNumChildComponents(); --i >= 0;)
    {
        if (ConnectorComponent* const c = dynamic_cast <ConnectorComponent*> (getChildComponent (i)))
            if (c->sourceFilterID == arc.sourceNode
                 && c->destFilterID == arc.destNode
                 && c->sourceFilterChannel == arc.sourcePort
                 && c->destFilterChannel == arc.destPort)
                return c;
    }

    return nullptr;
}

PinComponent* GraphEditorComponent::findPinAt (const int x, const int y) const
{
    for (int i = getNumChildComponents(); --i >= 0;)
    {
        if (FilterComponent* fc = dynamic_cast <FilterComponent*> (getChildComponent (i)))
        {
            if (PinComponent* pin = dynamic_cast <PinComponent*> (fc->getComponentAt (x - fc->getX(),
                                                                                      y - fc->getY())))
                return pin;
        }
    }

    return nullptr;
}

void GraphEditorComponent::resized()
{
    updateFilterComponents();
    updateConnectorComponents();
}

void GraphEditorComponent::changeListenerCallback (ChangeBroadcaster*)
{
    updateComponents();
}

void GraphEditorComponent::onGraphChanged()
{
    updateComponents();
}

void GraphEditorComponent::updateConnectorComponents()
{
    const ValueTree arcs = graph.getArcsValueTree();
    for (int i = getNumChildComponents(); --i >= 0;)
    {
        ConnectorComponent* const cc = dynamic_cast <ConnectorComponent*> (getChildComponent (i));
        if (cc != nullptr && cc != draggingConnector)
        {
            if (! Node::connectionExists (arcs, cc->sourceFilterID, (uint32) cc->sourceFilterChannel,
                                                cc->destFilterID, (uint32) cc->destFilterChannel))
            {
                delete cc;
            }
            else
            {
                cc->update();
            }
        }
    }
}

void GraphEditorComponent::updateFilterComponents()
{
    for (int i = getNumChildComponents(); --i >= 0;)
        if (FilterComponent* const fc = dynamic_cast <FilterComponent*> (getChildComponent (i)))
            fc->update();
}
    
void GraphEditorComponent::updateComponents()
{
    for (int i = graph.getNumConnections(); --i >= 0;)
    {
        const ValueTree c = graph.getConnectionValueTree (i);
        const Arc arc (Node::arcFromValueTree (c));
        ConnectorComponent* connector = getComponentForConnection (arc);
        
        if (connector == nullptr)
        {
            connector = new ConnectorComponent (graph);
            addAndMakeVisible (connector);
        }
        
        connector->setInput (arc.sourceNode, arc.sourcePort);
        connector->setOutput (arc.destNode, arc.destPort);
    }
    
    for (int i = graph.getNumNodes(); --i >= 0;)
    {
        const Node node (graph.getNode (i));
        FilterComponent* comp = getComponentForFilter (node.getNodeId());
        if (comp == nullptr)
        {
            comp = new FilterComponent (graph, node, verticalLayout);
            addAndMakeVisible (comp);
        }
    }

    resized();
}

void GraphEditorComponent::beginConnectorDrag (const uint32 sourceNode, const int sourceFilterChannel,
                                               const uint32 destNode, const int destFilterChannel,
                                               const MouseEvent& e)
{
    draggingConnector = dynamic_cast <ConnectorComponent*> (e.originalComponent);
    if (draggingConnector == nullptr)
        draggingConnector = new ConnectorComponent (graph);

    draggingConnector->setInput (sourceNode, sourceFilterChannel);
    draggingConnector->setOutput (destNode, destFilterChannel);

    addAndMakeVisible (draggingConnector);
    draggingConnector->toFront (false);

    dragConnector (e);
}

void GraphEditorComponent::dragConnector (const MouseEvent& e)
{
    const MouseEvent e2 (e.getEventRelativeTo (this));

    if (draggingConnector != nullptr)
    {
        draggingConnector->setTooltip (String::empty);

        int x = e2.x;
        int y = e2.y;

        if (PinComponent* const pin = findPinAt (x, y))
        {
            uint32 srcFilter = draggingConnector->sourceFilterID;
            int srcChannel   = draggingConnector->sourceFilterChannel;
            uint32 dstFilter = draggingConnector->destFilterID;
            int dstChannel   = draggingConnector->destFilterChannel;

            if (srcFilter == 0 && ! pin->isInput)
            {
                srcFilter = pin->filterID;
                srcChannel = pin->port;
            }
            else if (dstFilter == 0 && pin->isInput)
            {
                dstFilter = pin->filterID;
                dstChannel = pin->port;
            }
            
            if (graph.canConnect (srcFilter, srcChannel, dstFilter, dstChannel))
            {
                x = pin->getParentComponent()->getX() + pin->getX() + pin->getWidth() / 2;
                y = pin->getParentComponent()->getY() + pin->getY() + pin->getHeight() / 2;

                draggingConnector->setTooltip (pin->getTooltip());
            }
        }

        if (draggingConnector->sourceFilterID == 0)
            draggingConnector->dragStart (x, y);
        else
            draggingConnector->dragEnd (x, y);
    }
}

Component* GraphEditorComponent::createContainerForNode (GraphNodePtr node, bool useGenericEditor)
{
    if (AudioProcessorEditor* ed = createEditorForNode (node, useGenericEditor))
        if (Component* comp = wrapAudioProcessorEditor (ed, node))
            return comp;
    return nullptr;
}

Component* GraphEditorComponent::wrapAudioProcessorEditor(AudioProcessorEditor* ed, GraphNodePtr) { return ed; }

AudioProcessorEditor* GraphEditorComponent::createEditorForNode (GraphNodePtr node, bool useGenericEditor)
{
    ScopedPointer<AudioProcessorEditor> ui = nullptr;
    
    if (! useGenericEditor)
    {
        ui = node->getProcessor()->createEditorIfNeeded();
        
        if (ui == nullptr)
            useGenericEditor = true;
    }
    
    if (useGenericEditor)
        ui = new GenericAudioProcessorEditor (node->getProcessor());
    
    return (nullptr != ui) ? ui.release() : nullptr;
}

void GraphEditorComponent::endDraggingConnector (const MouseEvent& e)
{
    if (draggingConnector == nullptr)
        return;

    draggingConnector->setTooltip (String::empty);

    const MouseEvent e2 (e.getEventRelativeTo (this));

    uint32 srcFilter = draggingConnector->sourceFilterID;
    int srcChannel   = draggingConnector->sourceFilterChannel;
    uint32 dstFilter = draggingConnector->destFilterID;
    int dstChannel   = draggingConnector->destFilterChannel;

    draggingConnector = nullptr;

    if (PinComponent* const pin = findPinAt (e2.x, e2.y))
    {
        if (srcFilter == 0)
        {
            if (pin->isInput)
                return;

            srcFilter  = pin->filterID;
            srcChannel = pin->port;
        }
        else
        {
            if (! pin->isInput)
                return;

            dstFilter   = pin->filterID;
            dstChannel  = pin->port;
        }
        
        if (graph.isRootGraph())
        {
            ViewHelpers::postMessageFor (this, new AddConnectionMessage (srcFilter, (uint32)srcChannel,
                                                                         dstFilter, (uint32)dstChannel));
        }
        else
        {
            if (GraphNodePtr ptr = graph.getGraphNode())
                if (auto* proc = dynamic_cast<SubGraphProcessor*> (ptr->getAudioProcessor()))
                    proc->getController().addConnection (srcFilter, (uint32)srcChannel,
                                                         dstFilter, (uint32)dstChannel);
        }
    }
}

PluginWindow* GraphEditorComponent::getOrCreateWindowForNode (GraphNodePtr f, bool useGeneric)
{
    // FIXME:
    jassertfalse;
    return 0;
#if 0
    if (PluginWindow* window = PluginWindow::getWindowFor (f))
        return window;
    
    PluginWindow* w = PluginWindow::getFirstWindow();
    ScopedPointer<Component> c (createContainerForNode (f, useGeneric));
    
    if (! w && c) {
            w = PluginWindow::createWindowFor (f, c.get());
            if (w)
                c.release();
    }
    else if (w && c)
    {
        w->updateGraphNode (f, c.release());
    }

    return w;
#endif
}


bool GraphEditorComponent::isInterestedInDragSource (const SourceDetails& details)
{
    if (! details.description.isArray())
        return false;
    
    if (auto* a = details.description.getArray())
    {
        const var type (a->getFirst());
        return type == var ("element://dnd/plugin");
    }
    
    return false;
}

void GraphEditorComponent::itemDropped (const SourceDetails& details)
{
    if (const auto* a = details.description.getArray())
    {
        PluginDescription desc;
        desc.pluginFormatName = a->getUnchecked (1);
        desc.fileOrIdentifier = a->getUnchecked (2);
        createNewPlugin (&desc, details.localPosition.x, details.localPosition.y);
    }
}

bool shouldDrawDragImageWhenOver()
{
    return true;
}

void GraphEditorComponent::valueTreeChildAdded (ValueTree& parentTree, ValueTree& childWhichHasBeenAdded)
{
    if (childWhichHasBeenAdded.hasType (Tags::node) ||
        childWhichHasBeenAdded.hasType (Tags::arc) ||
        childWhichHasBeenAdded.hasType (Tags::nodes) ||
        childWhichHasBeenAdded.hasType (Tags::arcs))
    {
        updateComponents();
    }
}

class TooltipBar   : public Component,
                     private Timer
{
public:
    TooltipBar()
    {
        startTimer (100);
    }

    void paint (Graphics& g)
    {
        g.setFont (Font (getHeight() * 0.7f, Font::bold));
        g.setColour (Colours::black);
        g.drawFittedText (tip, 10, 0, getWidth() - 12, getHeight(), Justification::centredLeft, 1);
    }

    void timerCallback()
    {
        Component* const underMouse = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
        TooltipClient* const ttc = dynamic_cast <TooltipClient*> (underMouse);

        String newTip;

        if (ttc != nullptr && ! (underMouse->isMouseButtonDown() || underMouse->isCurrentlyBlockedByAnotherModalComponent()))
            newTip = ttc->getTooltip();

        if (newTip != tip)
        {
            tip = newTip;
            repaint();
        }
    }

private:
    String tip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TooltipBar)
};

}
