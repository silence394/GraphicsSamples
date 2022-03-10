# Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
from pathlib import Path

import os

import pytest
import torch
import random

from kaolin.io.obj import return_type
from kaolin.io import shapenet

SHAPENETV1_PATH = '/data/ShapeNetCore.v1'
SHAPENETV2_PATH = '/data/ShapeNetCore.v2'
SHAPENET_TEST_CATEGORY_SYNSETS = ['02691156']
SHAPENET_TEST_CATEGORY_LABELS = ['plane']
SHAPENET_TEST_CATEGORY_SYNSETS_2 = ['02958343']
SHAPENET_TEST_CATEGORY_LABELS_2 = ['car']
SHAPENET_TEST_CATEGORY_SYNSETS_MULTI = ['02691156', '02958343']
SHAPENET_TEST_CATEGORY_LABELS_MULTI = ['plane', 'car']

ALL_CATEGORIES = [
    None,
    SHAPENET_TEST_CATEGORY_SYNSETS,
    SHAPENET_TEST_CATEGORY_LABELS,
    SHAPENET_TEST_CATEGORY_SYNSETS_2,
    SHAPENET_TEST_CATEGORY_LABELS_2,
    SHAPENET_TEST_CATEGORY_SYNSETS_MULTI,
    SHAPENET_TEST_CATEGORY_LABELS_MULTI,
]

# Skip test in a CI environment
@pytest.mark.skipif(os.getenv('CI') == 'true', reason="CI does not have dataset")
@pytest.mark.parametrize('version', ['v1', 'v2'])
@pytest.mark.parametrize('categories', ALL_CATEGORIES)
@pytest.mark.parametrize('train', [True, False])
@pytest.mark.parametrize('with_materials', [True, False])
class TestShapeNet(object):

    @pytest.fixture(autouse=True)
    def shapenet_dataset(self, version, categories, train, with_materials):
        if version == 'v1':
            return shapenet.ShapeNetV1(root=SHAPENETV1_PATH,
                                       categories=categories,
                                       train=train,
                                       split=0.7,
                                       with_materials=with_materials)
        elif version == 'v2':
            return shapenet.ShapeNetV2(root=SHAPENETV2_PATH,
                                       categories=categories,
                                       train=train,
                                       split=0.7,
                                       with_materials=with_materials)
        else:
            raise ValueError(f"version {version} not recognized")

    @pytest.mark.parametrize('index', [0, -1, None, None])
    def test_basic_getitem(self, shapenet_dataset, index, with_materials):
        if index is None:
            index = random.randint(0, len(shapenet_dataset) - 1)
        assert len(shapenet_dataset) > 0

        item = shapenet_dataset[index]
        data = item.data
        attributes = item.attributes
        assert isinstance(data, return_type)
        assert isinstance(attributes, dict)

        assert isinstance(data.vertices, torch.Tensor)
        assert len(data.vertices.shape) == 2
        assert data.vertices.shape[0] > 0
        assert data.vertices.shape[1] == 3

        assert isinstance(data.faces, torch.LongTensor)
        assert len(data.faces.shape) == 2
        assert data.faces.shape[0] > 0
        assert data.faces.shape[1] == 3

        if with_materials:
            assert isinstance(data.uvs, torch.Tensor)
            assert len(data.uvs.shape) == 2
            assert data.uvs.shape[1] == 2

            assert isinstance(data.face_uvs_idx, torch.LongTensor)
            assert data.face_uvs_idx.shape == data.faces.shape
            assert isinstance(data.materials, list)
            assert len(data.materials) > 0
            assert isinstance(data.materials_order, torch.LongTensor)
            assert len(data.materials_order.shape) == 2
            assert data.materials_order.shape[0] > 0
            assert data.materials_order.shape[1] == 2
        else:
            assert data.uvs is None
            assert data.face_uvs_idx is None
            assert data.materials is None
            assert data.materials_order is None


        assert isinstance(attributes['name'], str)
        assert isinstance(attributes['path'], Path)
        assert isinstance(attributes['synset'], str)
        assert isinstance(attributes['labels'], list)

    @pytest.mark.parametrize('index', [-1, -2])
    def test_neg_index(self, shapenet_dataset, index, with_materials):

        assert len(shapenet_dataset) > 0

        gt_item = shapenet_dataset[len(shapenet_dataset) + index]
        gt_data = gt_item.data
        gt_attributes = gt_item.attributes

        item = shapenet_dataset[index]
        data = item.data
        attributes = item.attributes

        assert torch.equal(data.vertices, gt_data.vertices)
        assert torch.equal(data.faces, gt_data.faces)

        if with_materials:
            assert torch.equal(data.uvs, gt_data.uvs)
            assert torch.equal(data.face_uvs_idx, gt_data.face_uvs_idx)
            assert torch.equal(data.materials_order, gt_data.materials_order)
            for m, gt_m in zip(data.materials, gt_data.materials):
                assert m.keys() == gt_m.keys()
                for k in m.keys():
                    if k == 'name':
                        assert m[k] == gt_m[k]
                    else:
                        assert torch.equal(m[k], gt_m[k])
        assert attributes['name'] == gt_attributes['name']
        assert attributes['path'] == gt_attributes['path']
        assert attributes['synset'] == gt_attributes['synset']
        assert attributes['labels'] == gt_attributes['labels']
