import bpy
from mathutils import noise, Vector
import bmesh
from math import *
from random import random, randint


factor = 1.5 # How far up and down the landscape goes
snow_level = 10.0 #How high for snow
water_level = -.5 # What level is the water
frequency = 0.3 # affects gradient
height = 0.0 # Height offset of the landscape
subdivisions = 400
water_noise_effect = 10
water_noise_frequency = 10
water_wave_frequency = 400
wave_height = 1/200
octaves = 1
octave_effect = 1
shore_erosion = False

#----------------------------- Windmill -------------------------------
def windmill_mesh(x=0, y=0, z=0,  size=2):
    #main body and its texture
    bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=size, depth=size*4, location=(x, y, z+1.5*size))
    main_body = bpy.context.active_object
    # Only need one material
    material = bpy.data.materials.new(name=f"FaceMaterial")
    material.use_nodes = True
    texture = bpy.data.images.load("gray_brick.jpg")
    main_body.data.materials.append(material)
    bsdf_node = material.node_tree.nodes.get("Principled BSDF")
    texture_node = material.node_tree.nodes.new(type="ShaderNodeTexImage")
    texture_node.image = texture
    material.node_tree.links.new(texture_node.outputs["Color"], bsdf_node.inputs["Base Color"])
    
    
    # roof and its texture
    bpy.ops.mesh.primitive_cone_add(vertices=40, radius1=size*1.1, radius2=0.01, depth=size*2.5, location=(x, y, z + size*4.6))
    roof = bpy.context.active_object

    # create roof material and node setup (TexCoord -> Mapping -> ImageTexture -> Principled BSDF)
    material = bpy.data.materials.new(name=f"FaceMaterial")
    material.use_nodes = True
    texture = bpy.data.images.load("roof.jpg")
    roof.data.materials.append(material)
    bsdf_node = material.node_tree.nodes.get("Principled BSDF")
    texture_node = material.node_tree.nodes.new(type="ShaderNodeTexImage")
    texture_node.image = texture
    material.node_tree.links.new(texture_node.outputs["Color"], bsdf_node.inputs["Base Color"])

    bpy.ops.object.light_add(radius=5, type='POINT', location=(x, y, z + size*4.6+(size*2.5)/2+0.1))
    sun = bpy.context.active_object
    sun.data.energy = 1000
    sun.data.color = (1.0, 0.52, 0.52)


    #blades and their textures
    #blade1
    bpy.ops.mesh.primitive_cube_add(size=size, location=(x, y + size*1.1, z + size*3.7))
    blade1 = bpy.context.active_object
    blade1.scale=(size*0.4, size*6, size*0.1)
    material = bpy.data.materials.new(name=f"FaceMaterial")
    material.use_nodes = True
    texture = bpy.data.images.load("blade.jpg")
    blade1.data.materials.append(material)
    bsdf_node = material.node_tree.nodes.get("Principled BSDF")
    texture_node = material.node_tree.nodes.new(type="ShaderNodeTexImage")
    texture_node.image = texture
    material.node_tree.links.new(texture_node.outputs["Color"], bsdf_node.inputs["Base Color"])

    #blade2
    bpy.ops.mesh.primitive_cube_add(size=size, location=(x, y + size*1.11, z + size*3.7))
    blade2 = bpy.context.active_object
    blade2.scale=(size*0.4, size*6, size*0.1)
    material = bpy.data.materials.new(name=f"FaceMaterial")
    material.use_nodes = True
    texture = bpy.data.images.load("blade.jpg")
    blade2.data.materials.append(material)
    bsdf_node = material.node_tree.nodes.get("Principled BSDF")
    texture_node = material.node_tree.nodes.new(type="ShaderNodeTexImage")
    texture_node.image = texture
    material.node_tree.links.new(texture_node.outputs["Color"], bsdf_node.inputs["Base Color"])

   


    #blades animation
    blade1.rotation_euler=(radians(90), radians(45), radians(0))
    blade2.rotation_euler=(radians(90), -radians(45), radians(0))
    #animating the windmill blades
    # Frame 1
    blade1.rotation_euler[1] = 0
    blade1.keyframe_insert("rotation_euler", frame=1)

    blade2.rotation_euler[1] = radians(90)
    blade2.keyframe_insert("rotation_euler", frame=1)

    # Frame 360
    blade1.rotation_euler[1] = radians(360)
    blade1.keyframe_insert("rotation_euler", frame=360)

    blade2.rotation_euler[1] = radians(450)
    blade2.keyframe_insert("rotation_euler", frame=360)

    for obj in [blade1, blade2]:
        if obj.animation_data and obj.animation_data.action:
            fcurves = getattr(obj.animation_data.action, "fcurves", None)
            if fcurves:
                for fcurve in fcurves:
                    for kp in fcurve.keyframe_points:
                        kp.interpolation = "LINEAR"
    

        
    # parent blades and roof to main_body, preserving their world transforms
    for child in (blade1, blade2, roof):
        child.parent = main_body
        child.matrix_parent_inverse = main_body.matrix_world.inverted()

    # create a root empty, parent the whole windmill to it and preserve main_body world transform
    bpy.ops.object.empty_add(type='PLAIN_AXES', location=main_body.location)
    windmill_root = bpy.context.active_object
    windmill_root.name = "WindmillRoot"

    main_body.parent = windmill_root
    main_body.matrix_parent_inverse = windmill_root.matrix_world.inverted()

    # rotate the entire windmill by rotating the root (adjust angles as needed)
    windmill_root.rotation_euler = (0, 0, radians(90))

    return windmill_root
#-----------------tree------------------------------
def new_tree():

    verts = []
    faces = []
    face_uvs = []
    cylinder_sections = 30

    def make_end_ring(start, end, radius, start_instead=False):
        middle_vector = end - start
        # use Vector for axis
        axis_x = Vector((1, 0, 0))
        point_one = middle_vector.cross(axis_x).normalized() * radius
        point_two = middle_vector.cross(point_one).normalized() * radius
        ring_indices = []
        # create exactly cylinder_sections verts (no duplicate seam)
        for s in range(cylinder_sections):
            angle_a = s * 2 * pi / cylinder_sections
            edge_point = cos(angle_a) * point_one + sin(angle_a) * point_two + start
            if start_instead:
                verts.append(edge_point)
            else:
                verts.append(edge_point + middle_vector)
            ring_indices.append(len(verts) - 1)
        return ring_indices

    def make_section(start_ring, end_ring):
        # both rings expected length == cylinder_sections
        n = cylinder_sections
        for i in range(n):
            a = start_ring[i]
            b = end_ring[i]
            c = end_ring[(i + 1) % n]
            d = start_ring[(i + 1) % n]
            faces.append([a, b, c, d])
            # store uvs as (u0,v0),(u1,v1) per face (used later)
            face_uvs.append(((i / n, 0), (i / n, 1)))

    # draw_branch_section not used in this simplified pipeline; remove or leave empty

    def add_texture_coordinates(tree_mesh):
        # add UV layer and assign for quads only
        uv_layer = tree_mesh.uv_layers.new(name="Bark")
        for mpoly, fuv in zip(tree_mesh.polygons, face_uvs):
            loops = mpoly.loop_indices
            if len(loops) == 4:
                uv_layer.data[loops[0]].uv = fuv[0]
                uv_layer.data[loops[1]].uv = fuv[1]
                uv_layer.data[loops[2]].uv = (fuv[1][0] + (1 / cylinder_sections), fuv[1][1])
                uv_layer.data[loops[3]].uv = (fuv[0][0] + (1 / cylinder_sections), fuv[0][1])
            else:
                # fallback: set small square in center
                for li in loops:
                    uv_layer.data[li].uv = (0.5, 0.5)

    def draw_branch(start, end, radius, prev_pos, subdivisions, start_ring):
        # build a simple straight segmented branch (no extra subdivisions)
        section_length = (start - prev_pos).length
        continue_vector = section_length * (start - prev_pos).normalized()

        middle_of_straight_branch = (start + end) / 2
        end_of_continue = start + continue_vector
        half_continue = start + continue_vector / 2
        early_spot = (2 * start + middle_of_straight_branch + 2 * half_continue) / 5
        middle_spot = (2 * start + end + end_of_continue) / 4
        late_spot = (middle_spot + 2 * end + middle_of_straight_branch) / 4

        early_ring = make_end_ring(start, early_spot, radius * 1.2)
        middle_ring = make_end_ring(early_spot, middle_spot, radius * 1.1)
        late_ring = make_end_ring(middle_spot, late_spot, radius * 1.05)
        end_ring = make_end_ring(late_spot, end, radius)

        make_section(start_ring, early_ring)
        make_section(early_ring, middle_ring)
        make_section(middle_ring, late_ring)
        make_section(late_ring, end_ring)
        return end_ring

    def make_tree(pos, calls, prev_pos, prev_ring, stop_adjustment=1):
        if random() > stop_adjustment / calls:
            return
        branches = randint(1, 3)
        for i in range(branches):
            new_endpoint = Vector((pos.x + 2 * (random() - 0.5),
                                   pos.y + 2 * (random() - 0.5),
                                   pos.z + 1 + random()))
            new_end_ring = draw_branch(pos, new_endpoint, .3 / calls, prev_pos, 1, prev_ring)
            make_tree(new_endpoint, calls + 1, pos, new_end_ring, stop_adjustment=stop_adjustment)

    # root ring
    start_ring = make_end_ring(Vector((0, 0, 0)), Vector((0, 0, 2)), 0.3, start_instead=True)
    make_tree(Vector((0, 0, 0)), 1, Vector((0, 0, -2)), start_ring, 4)

    tree_mesh = bpy.data.meshes.new(name="TreeMesh")
    tree_mesh.from_pydata(verts, [], faces)
    add_texture_coordinates(tree_mesh)
    tree_mesh.validate(verbose=True)

    tree_object = bpy.data.objects.new(name="Tree", object_data=tree_mesh)
    bpy.context.collection.objects.link(tree_object)

    material = bpy.data.materials.new(name="TreeBark")
    material.use_nodes = True
    tree_object.data.materials.append(material)
    bsdf_node = material.node_tree.nodes.get("Principled BSDF")
    texture_node = material.node_tree.nodes.new(type="ShaderNodeTexImage")
    texture_node.image = bpy.data.images.load("bark.jpg")
    material.node_tree.links.new(texture_node.outputs["Color"], bsdf_node.inputs["Base Color"])

    return tree_object

#---------------------------beacon light-----------------------------
def setup_beacon(location=(0,0,0)):
    bpy.ops.mesh.primitive_cone_add(vertices=32, radius1=0.7, radius2=0.3, depth=5, location=location)
    beacon = bpy.context.active_object

    material = bpy.data.materials.new(name="beacon_material")
    material.use_nodes = True
    beacon.data.materials.append(material)
    bsdf_node = material.node_tree.nodes.get("Principled BSDF")
    texture_node = material.node_tree.nodes.new(type="ShaderNodeTexImage")
    texture_node.image = bpy.data.images.load("beacon.png")
    material.node_tree.links.new(texture_node.outputs["Color"], bsdf_node.inputs["Base Color"])
    bpy.ops.mesh.primitive_cone_add(vertices=32, radius1=0.3, radius2=0.5, depth=0.5, location=(location[0], location[1], location[2]+2.85))

    bpy.ops.object.light_add(radius=5, type='SPOT', location=(location[0], location[1]+0.3, location[2]+2.55))
    light = bpy.context.active_object
    light.data.energy = 5000
    light.data.color = (1.0, 0.95, 0.8)
    light.data.spot_size = radians(75)
    light.rotation_euler = (radians(90), 0, 0)

    # animate beacon light rotation
    light.rotation_euler[2] = radians(279)
    light.keyframe_insert("rotation_euler", frame=1)

    # Frame 180
    light.rotation_euler[2] = radians(639)
    light.keyframe_insert("rotation_euler", frame=180)

    #frame 360
    light.rotation_euler[2] = radians(999)
    light.keyframe_insert("rotation_euler", frame=360)



#-------------------------Huts-----------------------------
def make_hut(x,y,z, size):
    #hut base
    bpy.ops.mesh.primitive_cylinder_add(location=(x,y,z), radius=1.5*size, depth=1.5*size)
    cylinder = bpy.context.active_object

    material = bpy.data.materials.new(name="hut_material")
    material.use_nodes = True
    cylinder.data.materials.append(material)
    bsdf_node = material.node_tree.nodes.get("Principled BSDF")
    texture_node = material.node_tree.nodes.new(type="ShaderNodeTexImage")
    texture_node.image = bpy.data.images.load("hut.jpg")
    material.node_tree.links.new(texture_node.outputs["Color"], bsdf_node.inputs["Base Color"])


    #roof

    # Create cone
    bpy.ops.mesh.primitive_cone_add(radius1=1.5*size+0.2,radius2=0.01, depth=size*1.5+0.2, location=(x, y, z + 1.5*size))
    cone = bpy.context.active_object

    # Create material
    mat = bpy.data.materials.new(name="RoofMaterial")
    mat.use_nodes = True
    cone.data.materials.append(mat)

    # Use world coordinates for roof
    nodes = mat.node_tree.nodes
    tex_coord = nodes.new('ShaderNodeTexCoord')
    mapping = nodes.new('ShaderNodeMapping')
    texture = nodes.new('ShaderNodeTexImage')
    texture.image = bpy.data.images.load("hut_roof.jpg")

    # Connect: TexCoord -> Mapping -> Texture
    mat.node_tree.links.new(tex_coord.outputs['Object'], mapping.inputs['Vector'])
    mat.node_tree.links.new(mapping.outputs['Vector'], texture.inputs['Vector'])

    # Connect to shader
    bsdf = nodes['Principled BSDF']
    mat.node_tree.links.new(texture.outputs[0], bsdf.inputs['Base Color'])


#----------------------------- Landscape -------------------------------
def make_vertex_color_material():
    mat = bpy.data.materials.new(name="VertexColorMaterial")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    color_node = mat.node_tree.nodes.new(type='ShaderNodeVertexColor')
    color_node.layer_name = "Col"
    mat.node_tree.links.new(color_node.outputs['Color'], bsdf.inputs['Base Color'])
    return mat

# Delete objects
def delete_all_objects():
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.object.select_by_type(type='MESH')
    bpy.ops.object.delete()
    # Deselect all objects first
    bpy.ops.object.select_all(action='DESELECT')
    # Iterate through all objects in the scene
    for obj in bpy.context.scene.objects:
        # Check if the object is a light
        if obj.type == 'LIGHT':
            # Select the light object
            obj.select_set(True)
            # Delete all selected objects (which are now only lights)
            bpy.ops.object.delete()


def make_key(co):
    return (round(co[0], 3), round(co[1], 3))

delete_all_objects()

bpy.ops.mesh.primitive_grid_add(size=20.0, x_subdivisions=subdivisions, y_subdivisions=subdivisions, location=(0, 0, water_level))
obj = bpy.context.active_object
for v in obj.data.vertices:
    v.co.z += sin(noise.noise(v.co * water_noise_frequency)*water_noise_effect + v.co.x * water_wave_frequency) * wave_height
    

obj.data.materials.append(make_vertex_color_material())

bpy.ops.object.mode_set(mode='EDIT')
bm = bmesh.from_edit_mesh(obj.data)
bm.faces.ensure_lookup_table()

color_layer = bm.loops.layers.color.new("Col")
for i, face in enumerate(bm.faces):
    for loop in face.loops:
        loop[color_layer] = (0, 0, 1, .3)

bmesh.update_edit_mesh(obj.data)

bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.mesh.primitive_grid_add(size=20.0, x_subdivisions=subdivisions, y_subdivisions=subdivisions)
step = 20.0 / subdivisions

obj = bpy.context.active_object

v_heights = {}
for v in obj.data.vertices:
    v.co.z += height + (noise.noise(frequency * v.co) * factor)
    for o in range(octaves):
        v.co.z += noise.noise((2**(o+1))*frequency * v.co) * octave_effect * factor/(2**(o+2))
    v_heights[make_key((v.co.x, v.co.y))] = v.co.z



#for v in obj.data.vertices:
#    v.co.xy *= 1.0 + noise.noise(20*v.co)/5

obj.data.materials.append(make_vertex_color_material())

bpy.ops.object.mode_set(mode='EDIT')
bm = bmesh.from_edit_mesh(obj.data)
bm.faces.ensure_lookup_table()

color_layer = bm.loops.layers.color.new("Col")
face_color = (1, 1, 1, 1)
for i, face in enumerate(bm.faces):
    for loop in face.loops:
        elev = loop.vert.co.z
        slope = 0
        water_direction = 0
        if abs(loop.vert.co.y) < 9.75 and abs(loop.vert.co.x) < 9.75 :
            e1 = v_heights[make_key((loop.vert.co.x - step, loop.vert.co.y + step))]
            e2 = v_heights[make_key((loop.vert.co.x + step, loop.vert.co.y - step))]
            s1 = e1 - e2
            e1 = v_heights[make_key((loop.vert.co.x, loop.vert.co.y + step))]
            e2 = v_heights[make_key((loop.vert.co.x, loop.vert.co.y - step))]
            s2 = e1 - e2
            e1 = v_heights[make_key((loop.vert.co.x + step, loop.vert.co.y + step))]
            e2 = v_heights[make_key((loop.vert.co.x - step, loop.vert.co.y - step))]
            s3 = e1 - e2
            e1 = v_heights[make_key((loop.vert.co.x + step, loop.vert.co.y))]
            e2 = v_heights[make_key((loop.vert.co.x - step, loop.vert.co.y))]
            s4 = e1 - e2
            slopes = [s1, s2, s3, s4]
            slope_values = [abs(s) for s in slopes]
            slope = max(slope_values)
            max_index = slope_values.index(slope)
            water_direction = max_index * pi/4
            if(slopes[max_index] < 0):
                water_direction += pi
# For the adjustment:
#   We want to move the lower vertices of the face under the upper vertices
#   All the way would make this face vertical (might not want that)
#   Or we could make the lower vertices lower and the higher ones higher
#   Either way, we need to figure out which vertices the lower vertices are!
        if(shore_erosion):
            if elev > (water_level - .2) and elev - water_level < 0.1 and slope < 0.5:
                loop[color_layer] = (0, .4, .1, 1)
                loop.vert.co.z = v_heights[make_key((loop.vert.co.x, loop.vert.co.y))] - .1

        if slope > 0.2:
            loop[color_layer] = (.35, .35, .35, 1) 
        elif elev < water_level: # underwater
            loop[color_layer] = (0, 0, 1, 1)
        elif elev < snow_level:
            loop[color_layer] = (0, .4, .1, 1)
        else:
            loop[color_layer] = (1, 1, 1, 1)

bmesh.update_edit_mesh(obj.data)

bpy.ops.object.mode_set(mode='OBJECT')


windmill_mesh(x=8, y=-5, z=0.3, size=0.8)

tree1= new_tree()
tree1.location = (9, -7, 0)
tree1.scale = (0.3, 0.3, 0.3)

tree2= new_tree()
tree2.location = (-7.5, 3, 0.1)
tree2.scale = (0.3, 0.3, 0.3)

tree3= new_tree()
tree3.location = (7, 8, 0.3)
tree3.scale = (0.3, 0.3, 0.3)

setup_beacon(location=(-7, -8, 3))
make_hut(-4.5, -6, 0, 0.5)#good position
make_hut(8, -0.2, 0.4, 0.5)#good position
make_hut(0.1, 5, 1, 0.5)#good position
make_hut(5, -7, 0.4, 0.5)#good position

