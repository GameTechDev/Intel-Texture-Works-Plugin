// Description:
//	This script has been tried on Adobe Photoshop CC 2014.
//	The script runs on both Windows or Mac-OS versions of Photoshop.
//	The script should work with all color spaces and pixel depths
//	To use, have an image open and run the script from Photoshop's "File->Scripts" menu.
//	The currently open document should be a cubemap, each face should be in a layer
//       and have the appropriate name +Y,-Y,+X,-X,+Z,-Z. Without the proper naming it will not work.
//       It applies a Gaussian Blur Filter onto the CubeMap, mathcing also the edge pixels to alleviate discontiniouties between faces.
//       The original image reamins unatered, a new image is always created.
//
// Installation:
//	Place this script in your Photoshop Presets\Scripts directory and re-start Photoshop.
//	The script will then appear in your "File->Scripts..." menu items.
//	For more info on Photoshop scripting, see
//		http://partners.Adobe.com/public/developer/photoshop/devcenter.html
//

var gSquare = new UnitValue(256,"px");
var gZero = new UnitValue(0,"px");
var bluramount = 100;
var isBreak = false;
 
var windowResourcePopUp = "palette {  \
    orientation: 'column', \
    alignChildren: ['fill', 'top'],  \
    preferredSize:[300, 70], \
    text: 'CubeMapBlur',  \
    margins:15, \
    \
    bottomGroup: Group{ \
        txtMessage: StaticText {text:'Start Processing', size: [180,30], alignment:['center', 'center']} \
        cancelButton: Button { text: 'Cancel', properties:{name:'cancel'}, size: [60,24], alignment:['right', 'center'] }, \
    }\
}"

 // String containing info needed to build the ScriptUI window
 var windowResource = "dialog { \
			valuePanel: Panel { \
				orientation: 'column', \
				alignChildren: ['fill', 'top'], \
				text: 'Radius (1-200)', \
				valueText: EditText {}, \
				buttonsGroup: Group { \
					cancelButton: Button {text: 'cancel', properties: {name: 'cancel'}}, \
					okButton: Button {text: 'Ok', properties: {name: 'ok'}} \
				} \
			} \
		}"
 
var win;
 

function showPopUp(message)
{
    win.show();
    win.bottomGroup. txtMessage.text=message;
    app.refresh();   
}
function closePopUp()
{
    win.close();
}

function CloseAll(newDoc, tgtDoc, strtRulerUnits)
{
     newDoc.close(SaveOptions.DONOTSAVECHANGES);
     tgtDoc.close(SaveOptions.DONOTSAVECHANGES);
     if (strtRulerUnits != Units.PIXELS) 
         app.preferences.rulerUnits = strtRulerUnits;
}

function main_body()
{
    if (!app.documents.length > 0) 
    {   
        // stop if no document is opened.
        alert("Sorry, No Current Document");
    } 
    else 
    {  
        //Modal UI window
        win = new Window(windowResource);
        win.text = "CubeMap Gaussian Blur";
        win.valuePanel.valueText.active = true;

        // button callbacks
        win.valuePanel.buttonsGroup.cancelButton.onClick = function() 
        {
           bluramount=0;
           return win.close();
        };
        win.valuePanel.buttonsGroup.okButton.onClick = function() 
        {
          bluramount = Number(win.valuePanel.valueText.text);
          win.close();
        };

        win.center();
        win.show();
        
        //Create Non Modal UI mesage window
        win = new Window(windowResourcePopUp);
        win.bottomGroup.cancelButton.onClick = function()
        {
            isBreak = true;
            win.close();
        };

       
       if (bluramount <=0 || bluramount >200)
       return;
  
        var strtRulerUnits = app.preferences.rulerUnits;
        if (strtRulerUnits != Units.PIXELS) 
        {
            app.preferences.rulerUnits = Units.PIXELS; // selections are always in pixels
        }

        var origDoc = app.activeDocument;
        var newDoc;
        var tgtDoc;
        var w = origDoc.width;
        var h = origDoc.height;
        var bits = origDoc.bitsPerChannel;
        var frontFacesNames = new Array("+Z","-X","+X","-Y","+Y"); //have to be in this order center,left, right,bottom,top
        var faceTargetCoords = new Array (new Array(1,1), new Array(0,1), new Array(2,1), new Array(1,2), new Array(1,0));
        var frontFaceRotations = new Array(0,0,0,0,0);
        var rightFacesNames = new Array("+X","+Z","-Z","-Y","+Y");
        var rightFaceRotations = new Array(0,0,0,-90,90);
        var backFacesNames = new Array("-Z","+X","-X","-Y","+Y");
        var backFaceRotations = new Array(0,0,0,180,180);
        var leftFacesNames = new Array("-X","-Z","+Z","-Y","+Y");
        var leftFaceRotations = new Array(0,0,0,90,-90);
        var upFacesNames = new Array("+Y","-X","+X","+Z","-Z");
        var upFaceRotations = new Array(0,90,-90,0,180);
        var bottomFacesNames = new Array("-Y","-X","+X","-Z","+Z");
        var bottomFaceRotations = new Array(0,-90,90,180,0);
        
        if (origDoc.artLayers.length >= 6)
        {
            //-----------------Preparation-----------------------------------------------------------------
            gSquare = w; //store tile width
                                
            //create new document ready for horizoal cube map aligment
            newDoc = app.documents.add(gSquare*3,gSquare*3, origDoc.resolution, "cubeMapBlurr", NewDocumentMode.RGB, DocumentFill.TRANSPARENT);
            newDoc.bitsPerChannel = bits;

            tgtDoc = app.documents.add(gSquare,gSquare, origDoc.resolution,  " Blured"+origDoc.name, NewDocumentMode.RGB, DocumentFill.TRANSPARENT);
            tgtDoc.bitsPerChannel = bits; 

            showPopUp("Processing Face +Z (1/6)");
            BlurCubeMapFaceAlternate(frontFacesNames, faceTargetCoords, frontFaceRotations, bluramount, origDoc, newDoc, "Blur +Z", tgtDoc);
            closePopUp();
            if (isBreak)
            {
                CloseAll(newDoc, tgtDoc, strtRulerUnits);
                return;
            }

            showPopUp("Processing Face +X (2/6)");
            BlurCubeMapFaceAlternate(rightFacesNames, faceTargetCoords,rightFaceRotations, bluramount, origDoc, newDoc, "Blur +X", tgtDoc);
            closePopUp();
            if (isBreak)
            {
                CloseAll(newDoc, tgtDoc, strtRulerUnits);
                return;
            }

            showPopUp("Processing Face -X (3/6)");
            BlurCubeMapFaceAlternate(leftFacesNames, faceTargetCoords,leftFaceRotations, bluramount, origDoc, newDoc, "Blur -X", tgtDoc);
            closePopUp();
            if (isBreak)
            {
                CloseAll(newDoc, tgtDoc, strtRulerUnits);
                return;
            }

            showPopUp("Processing Face -Z (4/6)");
            BlurCubeMapFaceAlternate(backFacesNames, faceTargetCoords,backFaceRotations, bluramount, origDoc, newDoc, "Blur -Z", tgtDoc);
            closePopUp();
            if (isBreak)
            {
                CloseAll(newDoc, tgtDoc, strtRulerUnits);
                return;
            }

            showPopUp("Processing Face +Y (5/6)");
            BlurCubeMapFace(upFacesNames, faceTargetCoords,upFaceRotations, bluramount, origDoc, newDoc, "Blur +Y", tgtDoc);
            closePopUp();
            if (isBreak)
            {
                CloseAll(newDoc, tgtDoc, strtRulerUnits);
                return;
            }

            showPopUp("Processing Face -Y (6/6)");
            BlurCubeMapFace(bottomFacesNames, faceTargetCoords,bottomFaceRotations, bluramount, origDoc, newDoc, "Blur -Y", tgtDoc);
            closePopUp();   
            if (isBreak)
            {
                CloseAll(newDoc, tgtDoc, strtRulerUnits);
                return;
            }        

             showPopUp("FinalTouchUp");
             finalTouch(upFacesNames, faceTargetCoords, upFaceRotations, bluramount, tgtDoc, newDoc, "FinalTouch Up", tgtDoc);
             if (isBreak)
             {
                 CloseAll(newDoc, tgtDoc, strtRulerUnits);
                 return;
             }
        
             finalTouch(bottomFacesNames, faceTargetCoords, bottomFaceRotations, bluramount, tgtDoc, newDoc, "FinalTouch Up", tgtDoc);
             closePopUp();        
             if (isBreak)
             {
                 CloseAll(newDoc, tgtDoc, strtRulerUnits);
                 return;
             }
             
             //------------------------clean up------------------------------------------------------------
             //close it without saving
             newDoc.close(SaveOptions.DONOTSAVECHANGES);
             
             if (strtRulerUnits != Units.PIXELS) 
                 app.preferences.rulerUnits = strtRulerUnits;
        }
        else
        {
             alert("Please define 6 layers, one for each face of the cubemap with names -X,+X,-Z,+Z,-Y,+Y");
        }
        
    }
}

main_body();

function finalTouch(frontFacesNames, faceTargetCoords, frontFaceRotations, bluramount, origDoc, tmpDoc, descriptionStr, tgtDoc)
{
     //clear temp doc
     app.activeDocument = tmpDoc;
     app.activeDocument.activelayer = app.activeDocument.artLayers[0];
     app.activeDocument.activelayer.clear();
     
    //Copy specific face (the first in list) and all adjacent faces (next 4) for bluring
    var i=0;
    for (i=0; i<frontFacesNames.length; i++)
    {
        //start form src document and activate layer, select and copy
        app.activeDocument = origDoc;
        app.activeDocument.suspendHistory(descriptionStr+" step"+i, "CopyFromCubeMap(frontFacesNames, frontFaceRotations, i)");
      
        //activate dst document, select and past into selection
        app.activeDocument = tmpDoc;
        PasteToTempFile(faceTargetCoords, frontFaceRotations, i);
    }

     app.activeDocument = tmpDoc;
     app.activeDocument.mergeVisibleLayers();
     
     //Select area a bit larget than middle tile and apply artifact removal filter
     app.activeDocument.selection.select(selecthalf_document());
     app.activeDocument.activelayer = app.activeDocument.artLayers[0];
     app.activeDocument.activelayer.applyDustAndScratches(bluramount/2,0);
     app.activeDocument.selection.deselect();
     
    //copy all faces back into src doc
    for (i=0; i<frontFacesNames.length; i++)
    {
        app.activeDocument = tmpDoc;
        app.activeDocument.selection.select(horizontal_corners(faceTargetCoords[i]));
        app.activeDocument.selection.rotate(-frontFaceRotations[i]);   
        app.activeDocument.selection.copy();
        app.activeDocument.selection.deselect();
              
        app.activeDocument = tgtDoc;
        app.activeDocument.suspendHistory("paste "+frontFacesNames[i] , "PasteTouchedBlurredTile(frontFacesNames[i])");
    }
}

function PasteTouchedBlurredTile(layername)
{
       app.activeDocument.activeLayer =  app.activeDocument.artLayers.getByName(layername);
        app.activeDocument.selection.select(square_corners());
        var newLayer = app.activeDocument.paste();
        app.activeDocument.activelayer = newLayer;
        newLayer.merge();
}

function BlurCubeMapFaceAlternate(frontFacesNames, faceTargetCoords, frontFaceRotations, bluramount, origDoc, newDoc, descriptionStr, tgtDoc)
{
     //Copy specific face (the first in list) and all adjacent faces (next 4) for bluring
    var i=0;
    for (i=0; i<frontFacesNames.length; i++)
    {
        //start form src document and activate layer, select and copy
        app.activeDocument = origDoc;
        app.activeDocument.suspendHistory(descriptionStr+" step"+i, "CopyFromCubeMap(frontFacesNames, frontFaceRotations, i)");

        //activate dst document, select and past into selection
        app.activeDocument = newDoc;
        //app.activeDocument.suspendHistory("warpAdjacentFacesAndBlur", " PasteToTempFile(faceTargetCoords, frontFaceRotations, i)");
        PasteToTempFile(faceTargetCoords, frontFaceRotations, i);
        
        //revert to active src document
        activeDocument = origDoc;
    }
   
     app.activeDocument = newDoc;
     app.activeDocument.mergeVisibleLayers();
     
     //frontFaceTargetCoords[3] is bottom tile
     app.activeDocument.selection.select(horizontal_corners(faceTargetCoords[3]));
     app.activeDocument.selection.copy();     
     app.activeDocument.selection.deselect();
     app.activeDocument.selection.select(horizontal_corners(new Array(2,2)));
     var newLayer = activeDocument.paste();
     app.activeDocument.activelayer = newLayer;
     app.activeDocument.selection.select(horizontal_corners(new Array(2,2)));
     app.activeDocument.selection.rotate(-90);   
     app.activeDocument.selection.deselect();
     app.activeDocument.selection.select(horizontal_corners(new Array(0,2)));
     var newLayer = activeDocument.paste();
     app.activeDocument.activelayer = newLayer;
     app.activeDocument.selection.select(horizontal_corners(new Array(0,2)));
     app.activeDocument.selection.rotate(90);   
     app.activeDocument.selection.deselect();    
     app.activeDocument.mergeVisibleLayers();
     
     //frontFaceTargetCoords[4] is top tile
     app.activeDocument.selection.select(horizontal_corners(faceTargetCoords[4]));
     app.activeDocument.selection.copy();     
     app.activeDocument.selection.deselect();
     app.activeDocument.selection.select(horizontal_corners(new Array(2,0)));
     var newLayer = activeDocument.paste();
     app.activeDocument.activelayer = newLayer;
     app.activeDocument.selection.select(horizontal_corners(new Array(2,0)));
     app.activeDocument.selection.rotate(90);   
     app.activeDocument.selection.deselect();
     app.activeDocument.selection.select(horizontal_corners(new Array(0,0)));
     var newLayer = activeDocument.paste();
     app.activeDocument.activelayer = newLayer;
     app.activeDocument.selection.select(horizontal_corners(new Array(0,0)));
     app.activeDocument.selection.rotate(-90);   
     app.activeDocument.selection.deselect();    
     app.activeDocument.mergeVisibleLayers();
        
     app.activeDocument.activelayer = app.activeDocument.artLayers[0];
     app.activeDocument.activelayer.applyGaussianBlur(bluramount);

      //Copy center tile to tgdoc
      //frontFaceTargetCoords[0] is center tile
      app.activeDocument.selection.select(horizontal_corners(faceTargetCoords[0]));
      app.activeDocument.selection.copy();
      app.activeDocument.selection.deselect();
      
      //paste into correct cube map layer, frontFacesNames[0] is processed layer tile
      app.activeDocument = tgtDoc;
      app.activeDocument.suspendHistory("Paste "+descriptionStr, "PasteToCubeMapBlurredTile(frontFacesNames[0])");
}

function BlurCubeMapFace(frontFacesNames, faceTargetCoords, frontFaceRotations, bluramount, origDoc, newDoc, descriptionStr, tgtDoc)
{
     //Copy specific face (the first in list) and all adjacent faces (next 4) for bluring
    var i=0;
    for (i=0; i<frontFacesNames.length; i++)
    {
        //start form src document and activate layer, select and copy
        app.activeDocument = origDoc;
        app.activeDocument.suspendHistory(descriptionStr+" step"+i, "CopyFromCubeMap(frontFacesNames, frontFaceRotations, i)");

        //activate dst document, select and past into selection
        app.activeDocument = newDoc;
        //app.activeDocument.suspendHistory("warpAdjacentFacesAndBlur", " PasteToTempFile(faceTargetCoords, frontFaceRotations, i)");
        PasteToTempFile(faceTargetCoords, frontFaceRotations, i);
        
        //revert to active src document
        activeDocument = origDoc;
    }

     app.activeDocument = newDoc;
     app.activeDocument.mergeVisibleLayers();
     
     //warp and blur whole doc
     warpAdjacentFaces();
     
    for (i=0; i<frontFacesNames.length; i++)
    {
        //start form src document and activate layer, select and copy
        app.activeDocument = origDoc;
        app.activeDocument.suspendHistory(descriptionStr+" step"+i, "CopyFromCubeMap(frontFacesNames, frontFaceRotations, i)");

        //activate dst document, select and past into selection
        app.activeDocument = newDoc;
        //app.activeDocument.suspendHistory("warpAdjacentFacesAndBlur", " PasteToTempFile(faceTargetCoords, frontFaceRotations, i)");
        PasteToTempFile(faceTargetCoords, frontFaceRotations, i);
        
        //revert to active src document
        activeDocument = origDoc;
    }

    app.activeDocument = newDoc;
    app.activeDocument.mergeVisibleLayers();
    app.activeDocument.activelayer = app.activeDocument.artLayers[0];
    app.activeDocument.activelayer.applyGaussianBlur(bluramount);

      //Copy center tile
      //frontFaceTargetCoords[0] is center tile
      app.activeDocument.selection.select(horizontal_corners(faceTargetCoords[0]));
      app.activeDocument.selection.copy();
      app.activeDocument.selection.deselect();
      
      //paste into correct cube map layer, frontFacesNames[0] is processed layer tile
      app.activeDocument = tgtDoc;
      app.activeDocument.suspendHistory("Paste "+descriptionStr, "PasteToCubeMapBlurredTile(frontFacesNames[0])");
}

function PasteToCubeMapBlurredTile(layername)
{
      var newLayer = app.activeDocument.paste();
      newLayer.name = layername;     
}

function CopyFromCubeMap(frontFacesNames, frontFaceRotations, index)
{
     app.activeDocument.activeLayer =  app.activeDocument.artLayers.getByName(frontFacesNames[index]);
     app.activeDocument.selection.select(square_corners());
     app.activeDocument.selection.copy();
     app.activeDocument.selection.deselect();
}
function PasteToTempFile(faceTargetCoords, frontFaceRotations, index)
{
    app.activeDocument.selection.select(horizontal_corners(faceTargetCoords[index]));
    var newLayer = activeDocument.paste();
    app.activeDocument.activelayer = newLayer;
    app.activeDocument.selection.select(horizontal_corners(faceTargetCoords[index]));
    app.activeDocument.selection.rotate(frontFaceRotations[index]);   
    app.activeDocument.selection.deselect();
}
function warpAdjacentFaces()
{
    warpHorizontal(gSquare, 0, gSquare, gSquare*2, "left");
    warpHorizontal(gSquare, gSquare*2, gSquare*3, gSquare*2, "right");
    warpVertical(0, gSquare, gSquare*2, gSquare, "top");
    warpVertical(2*gSquare, gSquare, gSquare*2, gSquare*3, "bottom");
}


//The target doc is layed out in a  with the tile to blur in the middle and all others adjacent
//     X
// X T  X
//     X
function horizontal_corners(coords)
{
           var origin = new Array(gZero+coords[0]*gSquare, gZero+coords[1]*gSquare);
           var oX  = step_right(origin, gSquare);
           var oY  = step_down(origin, gSquare);
           var oXY = step_downright(origin, gSquare);
    
           return new Array(origin,oX,oXY,oY);
}
function selecthalf_document()
{
           var origin = new Array(gZero+0.95*gSquare, gZero+0.95*gSquare);
           var oX  = step_right(origin, 1.1*gSquare);
           var oY  = step_down(origin, 1.1*gSquare);
           var oXY = step_downright(origin, 1.1*gSquare);
    
           return new Array(origin,oX,oXY,oY);
}

//The original doc has one face per layer so the coords are at 0,0
function square_corners()
{
    var origin = new Array(gZero,gZero);
    var oX  = step_right(origin, gSquare);
    var oY  = step_down(origin, gSquare);
    var oXY =step_downright(origin, gSquare);
    
    return new Array(origin,oX,oXY,oY);
}

function step_right(corner,size) { return new Array(corner[0]+size,corner[1]); }

function step_down(corner,size) { return new Array(corner[0],corner[1]+size); }

function step_downright(corner,size) { return new Array(corner[0]+size,corner[1]+size); }


function warpHorizontal(topDistance, leftDistance, rightDistance, bottomDistance, type)
{
   var width = rightDistance - leftDistance;
   var increment = width/3;
   
   activeDocument.selection.select(new Array(new Array(leftDistance,topDistance), new Array(rightDistance,topDistance), new Array(rightDistance,bottomDistance),new Array(leftDistance,bottomDistance)));  
    
    var horizIncrements = new Array(0, increment, 2*increment, 3*increment);
    var vertIncrements = new Array(0, increment, 2*increment,  3*increment);
    
    if (type=="left")
        vertIncrements = new Array(3*increment, 2*increment, increment,  0);
    
   var idTrnf = charIDToTypeID( "Trnf" );
    var desc4 = new ActionDescriptor();
    var idnull = charIDToTypeID( "null" );
        var ref2 = new ActionReference();
        var idLyr = charIDToTypeID( "Lyr " );
        var idOrdn = charIDToTypeID( "Ordn" );
        var idTrgt = charIDToTypeID( "Trgt" );
        ref2.putEnumerated( idLyr, idOrdn, idTrgt );
    desc4.putReference( idnull, ref2 );
    var idFTcs = charIDToTypeID( "FTcs" );
    var idQCSt = charIDToTypeID( "QCSt" );
    var idQcsa = charIDToTypeID( "Qcsa" );
    desc4.putEnumerated( idFTcs, idQCSt, idQcsa );
    var idOfst = charIDToTypeID( "Ofst" );
        var desc5 = new ActionDescriptor();
        var idHrzn = charIDToTypeID( "Hrzn" );
        var idPxl = charIDToTypeID( "#Pxl" );
        desc5.putUnitDouble( idHrzn, idPxl, 0.000000 );
        var idVrtc = charIDToTypeID( "Vrtc" );
        var idPxl = charIDToTypeID( "#Pxl" );
        desc5.putUnitDouble( idVrtc, idPxl, 0.000000 );
    var idOfst = charIDToTypeID( "Ofst" );
    desc4.putObject( idOfst, idOfst, desc5 );
    var idwarp = stringIDToTypeID( "warp" );
        var desc6 = new ActionDescriptor();
        var idwarpStyle = stringIDToTypeID( "warpStyle" );
        var idwarpStyle = stringIDToTypeID( "warpStyle" );
        var idwarpCustom = stringIDToTypeID( "warpCustom" );
        desc6.putEnumerated( idwarpStyle, idwarpStyle, idwarpCustom );
        var idwarpValue = stringIDToTypeID( "warpValue" );
        desc6.putDouble( idwarpValue, 0.000000 );
        var idwarpPerspective = stringIDToTypeID( "warpPerspective" );
        desc6.putDouble( idwarpPerspective, 0.000000 );
        var idwarpPerspectiveOther = stringIDToTypeID( "warpPerspectiveOther" );
        desc6.putDouble( idwarpPerspectiveOther, 0.000000 );
        var idwarpRotate = stringIDToTypeID( "warpRotate" );
        var idOrnt = charIDToTypeID( "Ornt" );
        var idHrzn = charIDToTypeID( "Hrzn" );
        desc6.putEnumerated( idwarpRotate, idOrnt, idHrzn );
        var idbounds = stringIDToTypeID( "bounds" );
            var desc7 = new ActionDescriptor();
            var idTop = charIDToTypeID( "Top " );
            var idPxl = charIDToTypeID( "#Pxl" );
            desc7.putUnitDouble( idTop, idPxl, topDistance );
            var idLeft = charIDToTypeID( "Left" );
            var idPxl = charIDToTypeID( "#Pxl" );
            desc7.putUnitDouble( idLeft, idPxl, leftDistance );
            var idBtom = charIDToTypeID( "Btom" );
            var idPxl = charIDToTypeID( "#Pxl" );
            desc7.putUnitDouble( idBtom, idPxl, bottomDistance );
            var idRght = charIDToTypeID( "Rght" );
            var idPxl = charIDToTypeID( "#Pxl" );
            desc7.putUnitDouble( idRght, idPxl, rightDistance );
        var idRctn = charIDToTypeID( "Rctn" );
        desc6.putObject( idbounds, idRctn, desc7 );
        var iduOrder = stringIDToTypeID( "uOrder" );
        desc6.putInteger( iduOrder, 4 );
        var idvOrder = stringIDToTypeID( "vOrder" );
        desc6.putInteger( idvOrder, 4 );
        var idcustomEnvelopeWarp = stringIDToTypeID( "customEnvelopeWarp" );
            var desc8 = new ActionDescriptor();
            var idmeshPoints = stringIDToTypeID( "meshPoints" );
                var list1 = new ActionList();
                
                //1st row
                    var desc9 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc9.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[0] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc9.putUnitDouble( idVrtc, idPxl, topDistance - vertIncrements[0] );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc9 );
                    var desc10 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc10.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[1] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc10.putUnitDouble( idVrtc, idPxl, topDistance - vertIncrements[1]);
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc10 );
                    var desc11 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc11.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[2] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc11.putUnitDouble( idVrtc, idPxl, topDistance - vertIncrements[2] );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc11 );
                    var desc12 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc12.putUnitDouble( idHrzn, idPxl, rightDistance );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc12.putUnitDouble( idVrtc, idPxl, topDistance - vertIncrements[3] );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc12 );
                
                //2nd row
                    var desc13 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc13.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[0] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc13.putUnitDouble( idVrtc, idPxl, topDistance + increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc13 );
                    var desc14 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc14.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[1] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc14.putUnitDouble( idVrtc, idPxl,  topDistance + increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc14 );
                    var desc15 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc15.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[2] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc15.putUnitDouble( idVrtc, idPxl,  topDistance + increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc15 );
                    var desc16 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc16.putUnitDouble( idHrzn, idPxl, rightDistance );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc16.putUnitDouble( idVrtc, idPxl,  topDistance + increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc16 );
                
                //3rd row
                    var desc17 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc17.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[0] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc17.putUnitDouble( idVrtc, idPxl,  topDistance + 2*increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc17 );
                    var desc18 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc18.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[1] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc18.putUnitDouble( idVrtc, idPxl, topDistance + 2*increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc18 );
                    var desc19 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc19.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[2] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc19.putUnitDouble( idVrtc, idPxl, topDistance + 2*increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc19 );
                    var desc20 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc20.putUnitDouble( idHrzn, idPxl, rightDistance );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc20.putUnitDouble( idVrtc, idPxl, topDistance + 2*increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc20 );
                
                //4th row
                    var desc21 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc21.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[0] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc21.putUnitDouble( idVrtc, idPxl, bottomDistance + vertIncrements[0] );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc21 );
                    var desc22 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc22.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[1] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc22.putUnitDouble( idVrtc, idPxl, bottomDistance + vertIncrements[1] );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc22 );
                    var desc23 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc23.putUnitDouble( idHrzn, idPxl, leftDistance + horizIncrements[2] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc23.putUnitDouble( idVrtc, idPxl, bottomDistance + vertIncrements[2] );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc23 );
                    var desc24 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc24.putUnitDouble( idHrzn, idPxl, rightDistance );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc24.putUnitDouble( idVrtc, idPxl,  bottomDistance + vertIncrements[3]);
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc24 );
            desc8.putList( idmeshPoints, list1 );
        var idcustomEnvelopeWarp = stringIDToTypeID( "customEnvelopeWarp" );
        desc6.putObject( idcustomEnvelopeWarp, idcustomEnvelopeWarp, desc8 );
    var idwarp = stringIDToTypeID( "warp" );
    desc4.putObject( idwarp, idwarp, desc6 );
    var idIntr = charIDToTypeID( "Intr" );
    var idIntp = charIDToTypeID( "Intp" );
    var idBcbc = charIDToTypeID( "Bcbc" );
    desc4.putEnumerated( idIntr, idIntp, idBcbc );
    executeAction( idTrnf, desc4, DialogModes.NO );
    
    activeDocument.selection.deselect()
}

function warpVertical(topDistance, leftDistance, rightDistance, bottomDistance, type)
{  
   var width = rightDistance - leftDistance;
   var increment = width/3;
  
    activeDocument.selection.select(new Array(new Array(leftDistance,topDistance), new Array(rightDistance,topDistance), new Array(rightDistance,bottomDistance),new Array(leftDistance,bottomDistance)));
    
    var vertIncrements = new Array(0, increment, 2*increment, 3*increment);
    var horizIncrements = new Array(3*increment, 2*increment, increment,  0);
    
    if (type=="bottom")
       horizIncrements = new Array(0, increment, 2*increment,  3*increment);
    
   var idTrnf = charIDToTypeID( "Trnf" );
    var desc4 = new ActionDescriptor();
    var idnull = charIDToTypeID( "null" );
        var ref2 = new ActionReference();
        var idLyr = charIDToTypeID( "Lyr " );
        var idOrdn = charIDToTypeID( "Ordn" );
        var idTrgt = charIDToTypeID( "Trgt" );
        ref2.putEnumerated( idLyr, idOrdn, idTrgt );
    desc4.putReference( idnull, ref2 );
    var idFTcs = charIDToTypeID( "FTcs" );
    var idQCSt = charIDToTypeID( "QCSt" );
    var idQcsa = charIDToTypeID( "Qcsa" );
    desc4.putEnumerated( idFTcs, idQCSt, idQcsa );
    var idOfst = charIDToTypeID( "Ofst" );
        var desc5 = new ActionDescriptor();
        var idHrzn = charIDToTypeID( "Hrzn" );
        var idPxl = charIDToTypeID( "#Pxl" );
        desc5.putUnitDouble( idHrzn, idPxl, 0.000000 );
        var idVrtc = charIDToTypeID( "Vrtc" );
        var idPxl = charIDToTypeID( "#Pxl" );
        desc5.putUnitDouble( idVrtc, idPxl, 0.000000 );
    var idOfst = charIDToTypeID( "Ofst" );
    desc4.putObject( idOfst, idOfst, desc5 );
    var idwarp = stringIDToTypeID( "warp" );
        var desc6 = new ActionDescriptor();
        var idwarpStyle = stringIDToTypeID( "warpStyle" );
        var idwarpStyle = stringIDToTypeID( "warpStyle" );
        var idwarpCustom = stringIDToTypeID( "warpCustom" );
        desc6.putEnumerated( idwarpStyle, idwarpStyle, idwarpCustom );
        var idwarpValue = stringIDToTypeID( "warpValue" );
        desc6.putDouble( idwarpValue, 0.000000 );
        var idwarpPerspective = stringIDToTypeID( "warpPerspective" );
        desc6.putDouble( idwarpPerspective, 0.000000 );
        var idwarpPerspectiveOther = stringIDToTypeID( "warpPerspectiveOther" );
        desc6.putDouble( idwarpPerspectiveOther, 0.000000 );
        var idwarpRotate = stringIDToTypeID( "warpRotate" );
        var idOrnt = charIDToTypeID( "Ornt" );
        var idHrzn = charIDToTypeID( "Hrzn" );
        desc6.putEnumerated( idwarpRotate, idOrnt, idHrzn );
        var idbounds = stringIDToTypeID( "bounds" );
            var desc7 = new ActionDescriptor();
            var idTop = charIDToTypeID( "Top " );
            var idPxl = charIDToTypeID( "#Pxl" );
            desc7.putUnitDouble( idTop, idPxl, topDistance );
            var idLeft = charIDToTypeID( "Left" );
            var idPxl = charIDToTypeID( "#Pxl" );
            desc7.putUnitDouble( idLeft, idPxl, leftDistance );
            var idBtom = charIDToTypeID( "Btom" );
            var idPxl = charIDToTypeID( "#Pxl" );
            desc7.putUnitDouble( idBtom, idPxl, bottomDistance );
            var idRght = charIDToTypeID( "Rght" );
            var idPxl = charIDToTypeID( "#Pxl" );
            desc7.putUnitDouble( idRght, idPxl, rightDistance );
        var idRctn = charIDToTypeID( "Rctn" );
        desc6.putObject( idbounds, idRctn, desc7 );
        var iduOrder = stringIDToTypeID( "uOrder" );
        desc6.putInteger( iduOrder, 4 );
        var idvOrder = stringIDToTypeID( "vOrder" );
        desc6.putInteger( idvOrder, 4 );
        var idcustomEnvelopeWarp = stringIDToTypeID( "customEnvelopeWarp" );
            var desc8 = new ActionDescriptor();
            var idmeshPoints = stringIDToTypeID( "meshPoints" );
                var list1 = new ActionList();
                
                //1st row
                    var desc9 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc9.putUnitDouble( idHrzn, idPxl, leftDistance - horizIncrements[0] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc9.putUnitDouble( idVrtc, idPxl, topDistance);
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc9 );
                    var desc10 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc10.putUnitDouble( idHrzn, idPxl, leftDistance + increment );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc10.putUnitDouble( idVrtc, idPxl, topDistance);
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc10 );
                    var desc11 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc11.putUnitDouble( idHrzn, idPxl, leftDistance + 2*increment );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc11.putUnitDouble( idVrtc, idPxl, topDistance);
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc11 );
                    var desc12 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc12.putUnitDouble( idHrzn, idPxl, rightDistance + horizIncrements[0]);
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc12.putUnitDouble( idVrtc, idPxl, topDistance);
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc12 );
                
                //2nd row
                    var desc13 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc13.putUnitDouble( idHrzn, idPxl, leftDistance - horizIncrements[1] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc13.putUnitDouble( idVrtc, idPxl, topDistance + increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc13 );
                    var desc14 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc14.putUnitDouble( idHrzn, idPxl, leftDistance + increment );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc14.putUnitDouble( idVrtc, idPxl,  topDistance + increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc14 );
                    var desc15 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc15.putUnitDouble( idHrzn, idPxl, leftDistance + 2*increment );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc15.putUnitDouble( idVrtc, idPxl,  topDistance + increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc15 );
                    var desc16 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc16.putUnitDouble( idHrzn, idPxl, rightDistance + horizIncrements[1]  );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc16.putUnitDouble( idVrtc, idPxl,  topDistance + increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc16 );
                
                //3rd row
                    var desc17 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc17.putUnitDouble( idHrzn, idPxl, leftDistance - horizIncrements[2] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc17.putUnitDouble( idVrtc, idPxl,  topDistance + 2*increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc17 );
                    var desc18 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc18.putUnitDouble( idHrzn, idPxl, leftDistance +increment );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc18.putUnitDouble( idVrtc, idPxl, topDistance + 2*increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc18 );
                    var desc19 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc19.putUnitDouble( idHrzn, idPxl, leftDistance + 2*increment );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc19.putUnitDouble( idVrtc, idPxl, topDistance + 2*increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc19 );
                    var desc20 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc20.putUnitDouble( idHrzn, idPxl, rightDistance + horizIncrements[2] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc20.putUnitDouble( idVrtc, idPxl, topDistance + 2*increment );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc20 );
                
                //4th row
                    var desc21 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc21.putUnitDouble( idHrzn, idPxl, leftDistance - horizIncrements[3] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc21.putUnitDouble( idVrtc, idPxl, bottomDistance );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc21 );
                    var desc22 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc22.putUnitDouble( idHrzn, idPxl, leftDistance +increment );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc22.putUnitDouble( idVrtc, idPxl, bottomDistance);
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc22 );
                    var desc23 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc23.putUnitDouble( idHrzn, idPxl, leftDistance +2*increment );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc23.putUnitDouble( idVrtc, idPxl, bottomDistance );
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc23 );
                    var desc24 = new ActionDescriptor();
                    var idHrzn = charIDToTypeID( "Hrzn" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc24.putUnitDouble( idHrzn, idPxl, rightDistance + horizIncrements[3] );
                    var idVrtc = charIDToTypeID( "Vrtc" );
                    var idPxl = charIDToTypeID( "#Pxl" );
                    desc24.putUnitDouble( idVrtc, idPxl,  bottomDistance);
                var idrationalPoint = stringIDToTypeID( "rationalPoint" );
                list1.putObject( idrationalPoint, desc24 );
            desc8.putList( idmeshPoints, list1 );
        var idcustomEnvelopeWarp = stringIDToTypeID( "customEnvelopeWarp" );
        desc6.putObject( idcustomEnvelopeWarp, idcustomEnvelopeWarp, desc8 );
    var idwarp = stringIDToTypeID( "warp" );
    desc4.putObject( idwarp, idwarp, desc6 );
    var idIntr = charIDToTypeID( "Intr" );
    var idIntp = charIDToTypeID( "Intp" );
    var idBcbc = charIDToTypeID( "Bcbc" );
    desc4.putEnumerated( idIntr, idIntp, idBcbc );
    executeAction( idTrnf, desc4, DialogModes.NO );

    activeDocument.selection.deselect()
}

// eof

