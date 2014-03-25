/*!
 * \file SU2_GDC.cpp
 * \brief Main file of the Geometry Definition Code (SU2_GDC).
 * \author Aerospace Design Laboratory (Stanford University) <http://su2.stanford.edu>.
 * \version 3.0.0 "eagle"
 *
 * SU2, Copyright (C) 2012-2014 Aerospace Design Laboratory (ADL).
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/SU2_GDC.hpp"
using namespace std;

int main(int argc, char *argv[]) {
  
  /*--- Local variables ---*/
  
	unsigned short iDV, nZone = 1, iFFDBox, iPlane, nPlane, iVar;
	double *ObjectiveFunc, *ObjectiveFunc_New, *Gradient, delta_eps, MinPlane, MaxPlane, MinXCoord, MaxXCoord,
  **Plane_P0, **Plane_Normal;
  vector<double> *Xcoord_Airfoil, *Ycoord_Airfoil, *Zcoord_Airfoil, *Variable_Airfoil;
  char grid_file[200];

 	char *cstr;
	ofstream Gradient_file, ObjFunc_file;
	int rank = MASTER_NODE;
  
  /*--- MPI initialization ---*/

#ifndef NO_MPI
#ifdef WINDOWS
	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
#else
	MPI::Init(argc,argv);
	rank = MPI::COMM_WORLD.Get_rank();
#endif
#endif
	
	/*--- Pointer to different structures that will be used throughout the entire code ---*/
  
	CFreeFormDefBox** FFDBox = NULL;
	CConfig *config = NULL;
	CGeometry *boundary = NULL;
	CSurfaceMovement *surface_mov = NULL;
	
	/*--- Definition of the class for the definition of the problem ---*/
  
	if (argc == 2) config = new CConfig(argv[1], SU2_GDC, ZONE_0, nZone, 0, VERB_HIGH);
	else {
		strcpy (grid_file, "default.cfg");
		config = new CConfig(grid_file, SU2_GDC, ZONE_0, nZone, 0, VERB_HIGH);
	}
	
  /*--- Change the name of the input-output files for the parallel computation ---*/
  
#ifndef NO_MPI
	config->SetFileNameDomain(rank+1);
#endif
	
	/*--- Definition of the class for the boundary of the geometry ---*/
  
	boundary = new CBoundaryGeometry(config, config->GetMesh_FileName(), config->GetMesh_FileFormat());
  
  /*--- Set the number of sections, and allocate the memory ---*/
  
  if (boundary->GetnDim() == 2) nPlane = 1;
  else nPlane = config->GetnSections();

  Xcoord_Airfoil = new vector<double>[nPlane];
  Ycoord_Airfoil = new vector<double>[nPlane];
  Zcoord_Airfoil = new vector<double>[nPlane];
  Variable_Airfoil = new vector<double>[nPlane];

  Plane_P0 = new double*[nPlane];
  Plane_Normal = new double*[nPlane];
  for(iPlane = 0; iPlane < nPlane; iPlane++ ) {
    Plane_P0[iPlane] = new double[3];
    Plane_Normal[iPlane] = new double[3];
  }
  
  ObjectiveFunc = new double[nPlane*20];
  ObjectiveFunc_New = new double[nPlane*20];
  Gradient = new double[nPlane*20];

  for (iVar = 0; iVar < nPlane*20; iVar++) {
    ObjectiveFunc[iVar] = 0.0;
    ObjectiveFunc_New[iVar] = 0.0;
    Gradient[iVar] = 0.0;
  }
  
  /*--- Evaluation of the objective function ---*/
  
	if (rank == MASTER_NODE)
		cout << endl <<"----------------------- Preprocessing computations ----------------------" << endl;

  
  /*--- Boundary geometry preprocessing ---*/
  
	if (rank == MASTER_NODE) cout << "Identify vertices." <<endl;
	boundary->SetVertex();
  
  /*--- Compute elements surrounding points & points surrounding points ---*/
  
  if (rank == MASTER_NODE) cout << "Setting local point and element connectivity." << endl;
  boundary->SetEsuP();
	boundary->SetPsuP();
  boundary->SetEdges();

	/*--- Create the control volume structures ---*/
  
	if (rank == MASTER_NODE) cout << "Set boundary control volume structure." << endl;
	boundary->SetBoundControlVolume(config, ALLOCATE);
	
  /*--- Compute the surface curvature ---*/
  
  if (rank == MASTER_NODE) cout << "Compute the surface curvature." << endl;
  boundary->ComputeSurf_Curvature(config);
  
	/*--- Create plane structure ---*/
  
  if (rank == MASTER_NODE) cout << "Set plane structure." << endl;
  if (boundary->GetnDim() == 2) {
    MinXCoord = -1E6; MaxXCoord = 1E6;
    Plane_Normal[0][0] = 0.0;   Plane_P0[0][0] = 0.0;
    Plane_Normal[0][1] = 1.0;   Plane_P0[0][1] = 0.0;
    Plane_Normal[0][2] = 0.0;   Plane_P0[0][2] = 0.0;
  }
  else if (boundary->GetnDim() == 3) {
    MinPlane = config->GetSection_Location(0); MaxPlane = config->GetSection_Location(1);
    MinXCoord = -1E6; MaxXCoord = 1E6;
    for (iPlane = 0; iPlane < nPlane; iPlane++) {
      Plane_Normal[iPlane][0] = 0.0;    Plane_P0[iPlane][0] = 0.0;
      Plane_Normal[iPlane][1] = 0.0;    Plane_P0[iPlane][1] = 0.0;
      Plane_Normal[iPlane][2] = 0.0;    Plane_P0[iPlane][2] = 0.0;
      Plane_Normal[iPlane][config->GetAxis_Orientation()] = 1.0;
      Plane_P0[iPlane][config->GetAxis_Orientation()] = MinPlane + iPlane*(MaxPlane - MinPlane)/double(nPlane-1);
    }
  }

  /*--- Create airfoil section structure ---*/
  
  if (rank == MASTER_NODE) cout << "Set airfoil section structure." << endl;
  for (iPlane = 0; iPlane < nPlane; iPlane++) {
    boundary->ComputeAirfoil_Section(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, MinXCoord, MaxXCoord, NULL,
                                     Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], Variable_Airfoil[iPlane], true, config);
  }
  
  if (rank == MASTER_NODE)
    cout << endl <<"-------------------- Objective function evaluation ----------------------" << endl;

  if (rank == MASTER_NODE) {
    
    /*--- Evaluate objective function ---*/
    for (iPlane = 0; iPlane < nPlane; iPlane++) {

      if (Xcoord_Airfoil[iPlane].size() != 0) {
        
        cout << "\nSection " << (iPlane+1) << ". Plane (yCoord): " << Plane_P0[iPlane][1] << "." << endl;
        
        ObjectiveFunc[iPlane]           = boundary->Compute_MaxThickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], true);
        ObjectiveFunc[1*nPlane+iPlane]  = boundary->Compute_Thickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, 0.250000, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], true);
        ObjectiveFunc[2*nPlane+iPlane]  = boundary->Compute_Thickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, 0.333333, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], true);
        ObjectiveFunc[3*nPlane+iPlane]  = boundary->Compute_Thickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, 0.500000, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], true);
        ObjectiveFunc[4*nPlane+iPlane]  = boundary->Compute_Thickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, 0.666666, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], true);
        ObjectiveFunc[5*nPlane+iPlane]  = boundary->Compute_Thickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, 0.750000, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], true);
        ObjectiveFunc[6*nPlane+iPlane]  = boundary->Compute_Area(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], true);
        ObjectiveFunc[7*nPlane+iPlane]  = boundary->Compute_AoA(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], true);
        ObjectiveFunc[8*nPlane+iPlane]  = boundary->Compute_Chord(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], true);
        
        cout << "Maximum thickness: "   << ObjectiveFunc[iPlane] << "." << endl;
        cout << "1/4 chord thickness: " << ObjectiveFunc[1*nPlane+iPlane] << "." << endl;
        cout << "1/3 chord thickness: " << ObjectiveFunc[2*nPlane+iPlane] << "." << endl;
        cout << "1/2 chord thickness: " << ObjectiveFunc[3*nPlane+iPlane] << "." << endl;
        cout << "2/3 chord thickness: " << ObjectiveFunc[4*nPlane+iPlane] << "." << endl;
        cout << "3/4 chord thickness: " << ObjectiveFunc[5*nPlane+iPlane] << "." << endl;
        cout << "Area: "                << ObjectiveFunc[6*nPlane+iPlane] << "." << endl;
        cout << "Angle of attack: "     << ObjectiveFunc[7*nPlane+iPlane] << "." << endl;
        cout << "Chord: "               << ObjectiveFunc[8*nPlane+iPlane] << "." << endl;
        
      }
      
    }
    
    /*--- Write the objective function in a external file ---*/
		cstr = new char [config->GetObjFunc_Value_FileName().size()+1];
		strcpy (cstr, config->GetObjFunc_Value_FileName().c_str());
		ObjFunc_file.open(cstr, ios::out);
    ObjFunc_file << "TITLE = \"SU2_GDC Simulation\"" << endl;
    
    if (boundary->GetnDim() == 2) {
      ObjFunc_file << "VARIABLES = \"MAX_THICKNESS\",\"1/4_THICKNESS\",\"1/3_THICKNESS\",\"1/2_THICKNESS\",\"2/3_THICKNESS\",\"3/4_THICKNESS\",\"AREA\",\"AOA\",\"CHORD\"" << endl;
    }
    else if (boundary->GetnDim() == 3) {
      ObjFunc_file << "VARIABLES = ";
      for (iPlane = 0; iPlane < nPlane; iPlane++) ObjFunc_file << "\"MAX_THICKNESS_SEC"<< (iPlane+1) << "\", ";
      for (iPlane = 0; iPlane < nPlane; iPlane++) ObjFunc_file << "\"1/4_THICKNESS_SEC"<< (iPlane+1) << "\", ";
      for (iPlane = 0; iPlane < nPlane; iPlane++) ObjFunc_file << "\"1/3_THICKNESS_SEC"<< (iPlane+1) << "\", ";
      for (iPlane = 0; iPlane < nPlane; iPlane++) ObjFunc_file << "\"1/2_THICKNESS_SEC"<< (iPlane+1) << "\", ";
      for (iPlane = 0; iPlane < nPlane; iPlane++) ObjFunc_file << "\"2/3_THICKNESS_SEC"<< (iPlane+1) << "\", ";
      for (iPlane = 0; iPlane < nPlane; iPlane++) ObjFunc_file << "\"3/4_THICKNESS_SEC"<< (iPlane+1) << "\", ";
      for (iPlane = 0; iPlane < nPlane; iPlane++) ObjFunc_file << "\"AREA_SEC"<< (iPlane+1) << "\", ";
      for (iPlane = 0; iPlane < nPlane; iPlane++) ObjFunc_file << "\"AOA_SEC"<< (iPlane+1) << "\", ";
      for (iPlane = 0; iPlane < nPlane-1; iPlane++) ObjFunc_file << "\"CHORD_SEC"<< (iPlane+1) << "\", ";
      ObjFunc_file << "\"CHORD_SEC"<< (nPlane) << "\"" << endl;
    }
    
    ObjFunc_file << "ZONE T= \"Geometrical variables (value)\"" << endl;
    
    for (iPlane = 0; iPlane < nPlane*9-1; iPlane++)
      ObjFunc_file << ObjectiveFunc[iPlane] <<", ";
    ObjFunc_file << ObjectiveFunc[nPlane*9-1] << endl;
    
    ObjFunc_file.close();
    
	}
	
	if (config->GetGeometryMode() == GRADIENT) {
		
		/*--- Definition of the Class for surface deformation ---*/
		surface_mov = new CSurfaceMovement();
		
		/*--- Definition of the FFD deformation class ---*/
		FFDBox = new CFreeFormDefBox*[MAX_NUMBER_FFD];
		
		if (rank == MASTER_NODE)
			cout << endl <<"------------- Gradient evaluation using finite differences --------------" << endl;

		/*--- Write the gradient in a external file ---*/
		if (rank == MASTER_NODE) {
			cstr = new char [config->GetObjFunc_Grad_FileName().size()+1];
			strcpy (cstr, config->GetObjFunc_Grad_FileName().c_str());
			Gradient_file.open(cstr, ios::out);
		}
		
		for (iDV = 0; iDV < config->GetnDV(); iDV++) {
			
			/*--- Bump deformation for 2D problems ---*/
			if (boundary->GetnDim() == 2) {
				
        if (rank == MASTER_NODE)
          cout << "Perform 2D deformation of the surface." << endl;
        
        switch ( config->GetDesign_Variable(iDV) ) {
          case HICKS_HENNE : surface_mov->SetHicksHenne(boundary, config, iDV, true); break;
          case DISPLACEMENT : surface_mov->SetDisplacement(boundary, config, iDV, true); break;
          case ROTATION : surface_mov->SetRotation(boundary, config, iDV, true); break;
          case NACA_4DIGITS : surface_mov->SetNACA_4Digits(boundary, config); break;
          case PARABOLIC : surface_mov->SetParabolic(boundary, config); break;
        }
				
			}
			
			/*--- Free Form deformation for 3D problems ---*/
			else if (boundary->GetnDim() == 3) {
        
        /*--- Read the FFD information in the first iteration ---*/
        if (iDV == 0) {
          
          if (rank == MASTER_NODE) cout << "Read the FFD information from mesh file." << endl;
          
          /*--- Read the FFD information from the grid file ---*/
          surface_mov->ReadFFDInfo(boundary, config, FFDBox, config->GetMesh_FileName(), true);
          
          /*--- If the FFDBox was not defined in the input file ---*/
          if (!surface_mov->GetFFDBoxDefinition() && (rank == MASTER_NODE)) {
            cout << "The input grid doesn't have the entire FFD information!" << endl;
            cout << "Press any key to exit..." << endl;
            cin.get();
          }
          
          if (rank == MASTER_NODE)
            cout <<"-------------------------------------------------------------------------" << endl;
          
        }
        
        if (rank == MASTER_NODE) {
          cout << endl << "Design variable number "<< iDV <<"." << endl;
          cout << "Perform 3D deformation of the surface." << endl;
        }
        
        /*--- Apply the control point change ---*/
        for (iFFDBox = 0; iFFDBox < surface_mov->GetnFFDBox(); iFFDBox++) {
          
          switch ( config->GetDesign_Variable(iDV) ) {
            case FFD_CONTROL_POINT : surface_mov->SetFFDCPChange(boundary, config, FFDBox[iFFDBox], iFFDBox, iDV, true); break;
            case FFD_DIHEDRAL_ANGLE : surface_mov->SetFFDDihedralAngle(boundary, config, FFDBox[iFFDBox], iFFDBox, iDV, true); break;
            case FFD_TWIST_ANGLE : surface_mov->SetFFDTwistAngle(boundary, config, FFDBox[iFFDBox], iFFDBox, iDV, true); break;
            case FFD_ROTATION : surface_mov->SetFFDRotation(boundary, config, FFDBox[iFFDBox], iFFDBox, iDV, true); break;
            case FFD_CAMBER : surface_mov->SetFFDCamber(boundary, config, FFDBox[iFFDBox], iFFDBox, iDV, true); break;
            case FFD_THICKNESS : surface_mov->SetFFDThickness(boundary, config, FFDBox[iFFDBox], iFFDBox, iDV, true); break;
            case FFD_VOLUME : surface_mov->SetFFDVolume(boundary, config, FFDBox[iFFDBox], iFFDBox, iDV, true); break;
          }
          
          /*--- Recompute cartesian coordinates using the new control points position ---*/
          surface_mov->SetCartesianCoord(boundary, config, FFDBox[iFFDBox], iFFDBox);
          
        }
        
 			}
      
      /*--- Create airfoil structure ---*/
      for (iPlane = 0; iPlane < nPlane; iPlane++) {
        boundary->ComputeAirfoil_Section(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, MinXCoord, MaxXCoord, NULL,
                                         Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], Variable_Airfoil[iPlane], false, config);
      }
      
			/*--- Compute gradient ---*/
			if (rank == MASTER_NODE) {
        
        delta_eps = config->GetDV_Value(iDV);
        
        if (delta_eps == 0) {
          cout << "The finite difference steps is zero!!" << endl;
          cout << "Press any key to exit..." << endl;
          cin.get();
#ifdef NO_MPI
          exit(1);
#else
#ifdef WINDOWS
		  MPI_Abort(MPI_COMM_WORLD,1);
		  MPI_Finalize();
#else
          MPI::COMM_WORLD.Abort(1);
          MPI::Finalize();
#endif
#endif
        }

        for (iPlane = 0; iPlane < nPlane; iPlane++) {
          
          if (Xcoord_Airfoil[iPlane].size() != 0) {
            
            cout << "\nSection " << (iPlane+1) << ". Plane (yCoord): " << Plane_P0[iPlane][1] << "." << endl;
            
            ObjectiveFunc_New[iPlane] = boundary->Compute_MaxThickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], false);
            Gradient[iPlane] = (ObjectiveFunc_New[iPlane] - ObjectiveFunc[iPlane]) / delta_eps;
            
            ObjectiveFunc_New[1*nPlane + iPlane] = boundary->Compute_Thickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, 0.250000, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], false);
            Gradient[1*nPlane + iPlane] = (ObjectiveFunc_New[1*nPlane + iPlane] - ObjectiveFunc[1*nPlane + iPlane]) / delta_eps;
            
            ObjectiveFunc_New[2*nPlane + iPlane] = boundary->Compute_Thickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, 0.333333, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], false);
            Gradient[2*nPlane + iPlane] = (ObjectiveFunc_New[2*nPlane + iPlane] - ObjectiveFunc[2*nPlane + iPlane]) / delta_eps;
            
            ObjectiveFunc_New[3*nPlane + iPlane] = boundary->Compute_Thickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, 0.500000, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], false);
            Gradient[3*nPlane + iPlane] = (ObjectiveFunc_New[3*nPlane + iPlane] - ObjectiveFunc[3*nPlane + iPlane]) / delta_eps;
            
            ObjectiveFunc_New[4*nPlane + iPlane] = boundary->Compute_Thickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, 0.666666, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], false);
            Gradient[4*nPlane + iPlane] = (ObjectiveFunc_New[4*nPlane + iPlane] - ObjectiveFunc[4*nPlane + iPlane]) / delta_eps;
            
            ObjectiveFunc_New[5*nPlane + iPlane] = boundary->Compute_Thickness(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, 0.750000, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], false);
            Gradient[5*nPlane + iPlane] = (ObjectiveFunc_New[5*nPlane + iPlane] - ObjectiveFunc[5*nPlane + iPlane]) / delta_eps;
            
            ObjectiveFunc_New[6*nPlane + iPlane] = boundary->Compute_Area(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], false);
            Gradient[6*nPlane + iPlane] = (ObjectiveFunc_New[6*nPlane + iPlane] - ObjectiveFunc[6*nPlane + iPlane]) / delta_eps;
            
            ObjectiveFunc_New[7*nPlane + iPlane] = boundary->Compute_AoA(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], false);
            Gradient[7*nPlane + iPlane] = (ObjectiveFunc_New[7*nPlane + iPlane] - ObjectiveFunc[7*nPlane + iPlane]) / delta_eps;
            
            ObjectiveFunc_New[8*nPlane + iPlane] = boundary->Compute_Chord(Plane_P0[iPlane], Plane_Normal[iPlane], iPlane, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane], false);
            Gradient[8*nPlane + iPlane] = (ObjectiveFunc_New[8*nPlane + iPlane] - ObjectiveFunc[8*nPlane + iPlane]) / delta_eps;
            
            cout << "Maximum thickness gradient: "    << Gradient[iPlane] << "." << endl;
            cout << "1/4 chord thickness gradient: "  << Gradient[1*nPlane + iPlane] << "." << endl;
            cout << "1/3 chord thickness gradient: "  << Gradient[2*nPlane + iPlane] << "." << endl;
            cout << "1/2 chord thickness gradient: "  << Gradient[3*nPlane + iPlane] << "." << endl;
            cout << "2/3 chord thickness gradient: "  << Gradient[4*nPlane + iPlane] << "." << endl;
            cout << "3/4 chord thickness gradient: "  << Gradient[5*nPlane + iPlane] << "." << endl;
            cout << "Area gradient: "                 << Gradient[6*nPlane + iPlane] << "." << endl;
            cout << "Angle of attack gradient: "      << Gradient[7*nPlane + iPlane] << "." << endl;
            cout << "Chord gradient: "                << Gradient[8*nPlane + iPlane] << "." << endl;
            
          }
          
        }
 				
        if (iDV == 0) {
          Gradient_file << "TITLE = \"SU2_GDC Simulation\"" << endl;

          if (boundary->GetnDim() == 2) {
            Gradient_file << "VARIABLES = \"DESIGN_VARIABLE\",\"MAX_THICKNESS\",\"1/4_THICKNESS\",\"1/3_THICKNESS\",\"1/2_THICKNESS\",\"2/3_THICKNESS\",\"3/4_THICKNESS\",\"AREA\",\"AOA\",\"CHORD\"" << endl;
          }
          else if (boundary->GetnDim() == 3) {
            Gradient_file << "VARIABLES = \"DESIGN_VARIABLE\",";
            for (iPlane = 0; iPlane < nPlane; iPlane++) Gradient_file << "\"MAX_THICKNESS_SEC"<< (iPlane+1) << "\", ";
            for (iPlane = 0; iPlane < nPlane; iPlane++) Gradient_file << "\"1/4_THICKNESS_SEC"<< (iPlane+1) << "\", ";
            for (iPlane = 0; iPlane < nPlane; iPlane++) Gradient_file << "\"1/3_THICKNESS_SEC"<< (iPlane+1) << "\", ";
            for (iPlane = 0; iPlane < nPlane; iPlane++) Gradient_file << "\"1/2_THICKNESS_SEC"<< (iPlane+1) << "\", ";
            for (iPlane = 0; iPlane < nPlane; iPlane++) Gradient_file << "\"2/3_THICKNESS_SEC"<< (iPlane+1) << "\", ";
            for (iPlane = 0; iPlane < nPlane; iPlane++) Gradient_file << "\"3/4_THICKNESS_SEC"<< (iPlane+1) << "\", ";
            for (iPlane = 0; iPlane < nPlane; iPlane++) Gradient_file << "\"AREA_SEC"<< (iPlane+1) << "\", ";
            for (iPlane = 0; iPlane < nPlane; iPlane++) Gradient_file << "\"AOA_SEC"<< (iPlane+1) << "\", ";
            for (iPlane = 0; iPlane < nPlane-1; iPlane++) Gradient_file << "\"CHORD_SEC"<< (iPlane+1) << "\", ";
            Gradient_file << "\"CHORD_SEC"<< (nPlane) << "\"" << endl;
          }
          
          Gradient_file << "ZONE T= \"Geometrical variables (gradient)\"" << endl;
          
        }
        
        Gradient_file << (iDV) <<", ";
        for (iPlane = 0; iPlane < nPlane*9-1; iPlane++)
          Gradient_file << Gradient[iPlane] <<", ";
        Gradient_file << Gradient[nPlane*9-1] << endl;
        
				if (iDV != (config->GetnDV()-1)) cout <<"-------------------------------------------------------------------------" << endl;
				
			}

		}
		
		if (rank == MASTER_NODE)
			Gradient_file.close();
    
	}
	
  /*--- Finalize MPI parallelization ---*/
	
#ifndef NO_MPI
#ifdef WINDOWS
	MPI_Finalize();
#else
	MPI::Finalize();
#endif
#endif
	
  /*--- Deallocate memory ---*/
  
  delete [] Xcoord_Airfoil;
  delete [] Ycoord_Airfoil;
  delete [] Zcoord_Airfoil;
  
  delete [] ObjectiveFunc;
  delete [] ObjectiveFunc_New;
  delete [] Gradient;
  
  for(iPlane = 0; iPlane < nPlane; iPlane++ ) {
    delete Plane_P0[iPlane];
    delete Plane_Normal[iPlane];
  }
  delete [] Plane_P0;
  delete [] Plane_Normal;
  
  /*--- End solver ---*/
  
	if (rank == MASTER_NODE)
		cout << endl <<"------------------------- Exit Success (SU2_GDC) ------------------------" << endl << endl;

  
	return EXIT_SUCCESS;
	
}
