from struct import unpack
import numpy
import ospray

# Needs https://github.com/paulmelis/blender-ply-import
try:
    from readply import readply
    have_readply = True
except ImportError:
    have_readply = False

try:
    import tinyobjloader
    have_tinyobjloader = True
except ImportError:
    have_tinyobjloader = False
    
def determine_mesh_type(face_lengths):
    
    minn, maxn = numpy.min(face_lengths), numpy.max(face_lengths)
    
    if minn == maxn:
        assert minn in [3,4]
        mesh_type = 'pure-triangle' if minn == 3 else 'pure-quad'
    elif minn == 3 and maxn == 4:
        mesh_type = 'mixed-tris-and-quads'
    else:
        mesh_type = 'mixed-polygons'
        
    return mesh_type, minn, maxn
    
    
def read_stl(fname, force_subdivision_mesh=False):
    
    with open(fname, 'rb') as f:
        
        header = f.read(80)
        if header[:5] == b'solid':
            raise ValueError("Can't read ASCII STL files")
            
        num_triangles = unpack('<I', f.read(4))[0]
        print('%s: %d triangles' % (fname, num_triangles))
        
        normals = numpy.empty((num_triangles, 3), 'float32')
        vertices = numpy.empty((num_triangles*3, 3), 'float32')
        
        for i in range(num_triangles):
            
            normals[i] = unpack('<fff', f.read(12))
            vertices[3*i+0] = unpack('<fff', f.read(12))
            vertices[3*i+1] = unpack('<fff', f.read(12))
            vertices[3*i+2] = unpack('<fff', f.read(12))
            f.read(2)
            
        indices = numpy.arange(num_triangles*3, dtype='uint32')
        
        if not force_subdivision_mesh:
            mesh = ospray.Geometry('mesh')
            mesh.set_param('vertex.position', ospray.copied_data_constructor_vec(vertices))        
            mesh.set_param('vertex.normal', ospray.copied_data_constructor_vec(normals))        
            mesh.set_param('index', ospray.copied_data_constructor_vec(indices.reshape((-1, 3))))        
        # XXX borked
        #else:
        #    mesh = ospray.Geometry('subdivision')
        #    mesh.set_param('vertex.position', ospray.copied_data_constructor_vec(vertices))        
        #    mesh.set_param('index', ospray.copied_data_constructor(indices))            
        #    mesh.set_param('face', numpy.repeat(3, num_triangles*3).astype('uint32'))
            
        mesh.commit()
        
        return [mesh]

def read_ply(fname, force_subdivision_mesh=False):
    
    if not have_readply:
        print('Warning: readply module not found')
        return []

    plymesh = readply(fname, vertex_values_per_loop=False)

    print('%s: %d vertices, %d faces' % (fname, plymesh['num_vertices'], plymesh['num_faces']))

    #loop_start = plymesh['loop_start']
    loop_length = plymesh['loop_length']
    
    mesh_type, minn, maxn = determine_mesh_type(loop_length)

    print('Mesh type: %s' % mesh_type)
    if force_subdivision_mesh:
        print('Forcing subdivision mesh')
        
    vertices = plymesh['vertices']
    vertices = vertices.reshape((-1, 3))

    indices = plymesh['faces']

    if mesh_type.startswith('pure-') and not force_subdivision_mesh:
        mesh = ospray.Geometry('mesh')
        indices = indices.reshape((-1, minn))
        mesh.set_param('index', ospray.copied_data_constructor_vec(indices))
        # Get a OSPRAY STATUS: ospray::Mesh ignoring 'index' array with wrong element type (should be vec3ui)
        # when passing a pure-quad index array
        
    elif mesh_type == 'mixed-tris-and-quads' and not force_subdivision_mesh:
        mesh = ospray.Geometry('mesh')
        
        # Duplicate last index of triangles to get all quads
        new_indices = []
        first = 0
        for n in loop_length:
            if n == 3:
                new_indices.extend(indices[first:first+3])
                new_indices.append(indices[first+2])
            else:
                new_indices.extend(indices[first:first+4])
            first += n
            
        new_indices = numpy.array(new_indices, dtype=numpy.uint32).reshape((-1, 4))    
        mesh.set_param('index', ospray.copied_data_constructor_vec(new_indices))
    else:
        # Use subdivision surface
        mesh = ospray.Geometry('subdivision')
        mesh.set_param('index', ospray.copied_data_constructor(indices))
        mesh.set_param('face', loop_length)
        
    mesh.set_param('vertex.position', ospray.copied_data_constructor_vec(vertices))

    if 'vertex_colors' in plymesh:
        print('Have vertex colors')
        colors = plymesh['vertex_colors'].reshape((-1, 3))
        
        # RGB -> RGBA 
        n = colors.shape[0]
        alpha = numpy.ones((n,1), dtype=numpy.float32)
        colors = numpy.hstack((colors, alpha))
        
        mesh.set_param('vertex.color', ospray.copied_data_constructor_vec(colors))

    mesh.commit()
    
    return [mesh]

    
def read_obj(fname, force_subdivision_mesh=False):
    
    if not have_tinyobjloader:
        print('Warning: tinyobjloader module not found')
        return []
    
    r = tinyobjloader.ObjReader()
    r.ParseFromFile(fname)

    attributes = r.GetAttrib()
    
    vertices = numpy.array(attributes.vertices)
    vertices = vertices.reshape((-1, 3)).astype('float32')
    
    shapes = r.GetShapes()
    
    print('%s: %d vertices, %d shapes' % (fname, vertices.shape[0], len(shapes)))
    
    meshes = []

    for s in r.GetShapes():

        mesh = s.mesh

        indices = mesh.numpy_indices()      # [ v0, n0, t0, v1, n1, t1, ... ]
        vertex_indices = indices[::3]
        
        face_lengths = mesh.numpy_num_face_vertices()
        
        mesh_type, minn, maxn = determine_mesh_type(face_lengths)
        
        print('%s (%s): %d faces' % (s.name, mesh_type, face_lengths.shape[0]))
        
        if mesh_type.startswith('pure-') and not force_subdivision_mesh:
            mesh = ospray.Geometry('mesh')
            vertex_indices = vertex_indices.reshape((-1, minn)).astype('uint32')
            mesh.set_param('index', ospray.copied_data_constructor_vec(vertex_indices))
        elif mesh_type == 'mixed-tris-and-quads' and not force_subdivision_mesh:
            mesh = ospray.Geometry('mesh')
            
            # Duplicate last index of triangles to get all quads
            new_indices = []
            first = 0
            for n in loop_length:
                if n == 3:
                    new_indices.extend(vertex_indices[first:first+3])
                    new_indices.append(vertex_indices[first+2])
                else:
                    new_indices.extend(vertex_indices[first:first+4])
                first += n
                
            new_indices = numpy.array(new_indices, dtype=numpy.uint32).reshape((-1, 4))    
            mesh.set_param('index', ospray.copied_data_constructor_vec(new_indices))
        else:
            # Use subdivision surface
            mesh = ospray.Geometry('subdivision')
            mesh.set_param('index', ospray.copied_data_constructor(vertex_indices))
            mesh.set_param('face', face_lengths)
            
        mesh.set_param('vertex.position', ospray.copied_data_constructor_vec(vertices))
        
        mesh.commit()
        
        meshes.append(mesh)
        
    return meshes
        
        
        
def read_pdb(fname, radius=1):
    """Bare-bones pdb reader, ATOM only"""
    
    COLORS = {
        'H' : (0xff, 0xff, 0xff),
        'C' : (0x00, 0x00, 0x00),
        'N' : (0x22, 0x33, 0xff),
        'O' : (0xff, 0x20, 0x00),
        'CL': (0x1f, 0xf0, 0x1f),
        'F' : (0x1f, 0xf0, 0x1f),
        'BR': (0x99, 0x22, 0x00),
        'I' : (0x66, 0x00, 0xbb),
        'HE': (0x00, 0xff, 0xff),
        'NE': (0x00, 0xff, 0xff),
        'AR': (0x00, 0xff, 0xff),
        'XE': (0x00, 0xff, 0xff),
        'KR': (0x00, 0xff, 0xff),
        'P' : (0xff, 0x99, 0x00),
        'S' : (0xff, 0xe5, 0x22),
        'B' : (0xff, 0xaa, 0x77),
        
        'TI': (0x99, 0x99, 0x99),
        'FE': (0xdd, 0x77, 0x00),
        
    }
        
    with open(fname, 'rt') as f:
        
        positions = []
        radii = []
        colors = []
        
        N = 0
        for line in f:
            if not line.startswith('ATOM '):
                continue
                
            x = float(line[30:38])
            y = float(line[38:46])
            z = float(line[46:54])
            element = line[76:78].strip()
                        
            positions.append((x, y, z))      
            
            if element in COLORS:
                hexcol = COLORS[element]
            else:
                hexcol = (0xdd, 0x77, 0xff)
                if element != '':
                    print('No color for element %s' % element)
                    
            colors.append((hexcol[0]/255, hexcol[1]/255, hexcol[2]/255))
                
            radii.append(radius)
            
            N += 1
            
        positions = numpy.array(positions, numpy.float32)
        radii = numpy.array(radii, numpy.float32)
        
        colors = numpy.array(colors, numpy.float32)
        #print(colors.shape, numpy.min(colors), numpy.max(colors))
        opacities = numpy.ones(N, 'float32')
        colors = numpy.c_[ colors, opacities ]
        
        #positions[:9].tofile('pos9.bin')
        #colors[:9].tofile('col9.bin')
        #radii[:9].tofile('radii9.bin')
        
        assert positions.shape[0] == colors.shape[0]

        spheres = ospray.Geometry('sphere')
        spheres.set_param('sphere.position', ospray.copied_data_constructor_vec(positions))
        spheres.set_param('sphere.radius', ospray.copied_data_constructor(radii))
        spheres.commit()
        
        gmodel = ospray.GeometricModel(spheres)
        gmodel.set_param('color', ospray.copied_data_constructor_vec(colors))
        gmodel.commit()
                
        return gmodel
