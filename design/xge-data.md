## Extension Names

* .xmd - meta data (similar to unity's .meta, we are changing .xgm to .xmd)
* .xc<> - composition: includes parameters, pure refences to *.xd<>, these are authored by users on their respective editors within xgEditor, staged in Assets
    * .xco - object (mesh + materials + hierarchy + animation + physics), a renderer/drawer component uses these
    * .xcm - material (shaders + textures)
    * .xcf - texture flipbook
    * .xca - texture atlas
* .xd<> - processed data
    * .xdm - mesh (vertex data are in stride, ie. pos + uv + normal + tangent + binormal)
    * .xdh - hierarchy (+skin)
    * .xdt - texture
    * .xda - animation
* .xr<> - raw data: imported from FBX, obj, or external support formats
    * .xrm - raw mesh+submeshes (vertex data are in separated chunks)
    * .xrh - raw hierarchy
    etc...

## How it woks

* Import settings from .xmd will be used to generate .xr<>. 
* It will use the same name as the source files (dog.fbx, cat.png)
* They will become dog.fbx.xrm, cat.png.xrt(I am still thinking if it is benefitial to store this raw).
* All the .xd<> and .xr<> are unstaged in Library.
* References mean GUID references (file GUID and chunk GUID).
* .xc<> will have their own editor, users will be able to connect things via names (GUID are visible, but names are better for workflow)
* Name to guid tree is stored in .xmd of .xc<>, user can delete Library and able to reimport/reconstruct everything